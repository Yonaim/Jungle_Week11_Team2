#include "AssetEditor/AssetEditorManager.h"
#include "PCH/LunaticPCH.h"

#include "AssetEditor/CameraModifierStack/CameraModifierStackEditor.h"
#include "AssetEditor/IAssetEditor.h"
#include "AssetEditor/SkeletalMesh/SkeletalMeshEditor.h"
#include "AssetEditor/Window/AssetEditorWindow.h"
#include "Common/File/EditorFileUtils.h"
#include "Core/Notification.h"
#include "EditorEngine.h"
#include "Engine/Asset/AssetData.h"
#include "Engine/Asset/AssetFileSerializer.h"
#include "Engine/Mesh/SkeletalMesh.h"
#include "Mesh/MeshAssetManager.h"
#include "Mesh/StaticMesh.h"
#include "Object/Object.h"
#include "Platform/Paths.h"
#include "Render/Pipeline/Renderer.h"
#include "Serialization/WindowsArchive.h"

#include <algorithm>
#include <cwctype>

namespace
{
    std::wstring ToLowerExtension(const std::filesystem::path &Path)
    {
        std::wstring Extension = Path.extension().wstring();
        std::transform(Extension.begin(), Extension.end(), Extension.begin(),
                       [](wchar_t Ch) { return static_cast<wchar_t>(std::towlower(Ch)); });
        return Extension;
    }

    bool IsUnderDirectory(const std::filesystem::path &ChildPath, const std::filesystem::path &ParentPath)
    {
        const std::filesystem::path Child = ChildPath.lexically_normal();
        const std::filesystem::path Parent = ParentPath.lexically_normal();
        std::filesystem::path       Relative = Child.lexically_relative(Parent);
        if (Relative.empty())
        {
            return Child == Parent;
        }
        const std::wstring Native = Relative.native();
        return Native.rfind(L"..", 0) != 0 && !Relative.is_absolute();
    }

    std::filesystem::path MakeAbsoluteProjectPath(const std::filesystem::path &Path)
    {
        return std::filesystem::path(FPaths::ResolvePathToDisk(FPaths::ToUtf8(Path.generic_wstring()))).lexically_normal();
    }

    void ShowUnsupportedAssetEditorAlert(UEditorEngine *EditorEngine, const std::filesystem::path &AssetPath)
    {
        const std::wstring AssetName = AssetPath.empty() ? L"Unknown asset" : AssetPath.filename().wstring();
        const std::wstring Message = L"No editor is currently implemented for this asset type.\n\n" + AssetName;
        void              *OwnerWindowHandle = nullptr;
        if (EditorEngine && EditorEngine->GetWindow())
        {
            OwnerWindowHandle = EditorEngine->GetWindow()->GetHWND();
        }
        MessageBoxW(static_cast<HWND>(OwnerWindowHandle), Message.c_str(), L"Unsupported Asset Type", MB_OK | MB_ICONINFORMATION);
    }

    bool IsSupportedAssetEditorClassId(EAssetClassId ClassId)
    {
        return ClassId == EAssetClassId::SkeletalMesh || ClassId == EAssetClassId::CameraModifierStack;
    }

    void NotifyUnsupportedAssetEditor(UEditorEngine *EditorEngine, const std::filesystem::path &AssetPath)
    {
        const FString AssetPathUtf8 = FPaths::ToUtf8(AssetPath.generic_wstring());
        UE_LOG_CATEGORY(AssetEditor, Info, "[AssetEditor] Unsupported asset editor type: %s", AssetPathUtf8.c_str());
        FNotificationManager::Get().AddNotification("This asset type is not supported by the editor yet", ENotificationType::Info, 4.0f);
        ShowUnsupportedAssetEditorAlert(EditorEngine, AssetPath);
    }

} // namespace

void FAssetEditorManager::Initialize(UEditorEngine *InEditorEngine, FRenderer *InRenderer)
{
    EditorEngine = InEditorEngine;
    Renderer = InRenderer;
}

void FAssetEditorManager::Shutdown()
{
    AssetEditorWindow.Shutdown();
    Renderer = nullptr;
    EditorEngine = nullptr;
}

void FAssetEditorManager::Tick(float DeltaTime) { AssetEditorWindow.Tick(DeltaTime); }

void FAssetEditorManager::RenderContent(float DeltaTime, ImGuiID DockspaceId) { AssetEditorWindow.RenderContent(DeltaTime, DockspaceId); }

bool FAssetEditorManager::OpenAssetFromPath(const std::filesystem::path &AssetPath)
{
    const std::wstring Extension = ToLowerExtension(AssetPath);
    if (Extension != L".uasset")
    {
        FNotificationManager::Get().AddNotification("Asset Editor can only open .uasset files. Use Import Asset first.",
                                                    ENotificationType::Info, 4.0f);
        return false;
    }

    const std::filesystem::path NormalizedPath = MakeAbsoluteProjectPath(AssetPath);
    const std::filesystem::path ContentRoot = std::filesystem::path(FPaths::ContentDir()).lexically_normal();
    if (!IsUnderDirectory(NormalizedPath, ContentRoot))
    {
        FNotificationManager::Get().AddNotification(
            "Asset Editor opens only .uasset files under Asset/Content. Use Import Source Asset first.", ENotificationType::Error, 5.0f);
        return false;
    }

    if (AssetEditorWindow.ActivateTabByAssetPath(NormalizedPath))
    {
        AssetEditorWindow.Show();
        if (EditorEngine)
        {
            EditorEngine->SetActiveEditorContext(EEditorContextType::AssetEditor);
        }
        return true;
    }

    UObject *LoadedAsset = nullptr;
    FString  Error;

    FAssetFileHeader Header;
    if (FAssetFileSerializer::ReadAssetHeader(NormalizedPath, Header, &Error))
    {
        if (!IsSupportedAssetEditorClassId(Header.ClassId))
        {
            NotifyUnsupportedAssetEditor(EditorEngine, NormalizedPath);
            return false;
        }

        ID3D11Device *Device = Renderer ? Renderer->GetFD3DDevice().GetDevice() : nullptr;
        const FString AssetPathUtf8 = FPaths::ToUtf8(NormalizedPath.generic_wstring());

        if (Header.ClassId == EAssetClassId::SkeletalMesh)
        {
            LoadedAsset = FMeshAssetManager::LoadSkeletalMeshAssetFile(AssetPathUtf8, Device, EMeshAssetLoadPurpose::FreshInstance);
        }
    }

    if (!LoadedAsset)
    {
        LoadedAsset = FAssetFileSerializer::LoadObjectFromAssetFile(NormalizedPath, &Error);
    }

    if (!LoadedAsset)
    {
        FNotificationManager::Get().AddNotification(Error.empty() ? "Failed to load asset." : Error, ENotificationType::Error, 5.0f);
        return false;
    }

    if (OpenOwnedWorkingCopy(LoadedAsset, NormalizedPath))
    {
        return true;
    }

    UObjectManager::Get().DestroyObject(LoadedAsset);
    return false;
}

bool FAssetEditorManager::OpenOwnedWorkingCopy(UObject *Asset, const std::filesystem::path &AssetPath)
{
    if (!Asset)
    {
        return false;
    }

    const std::filesystem::path NormalizedPath = MakeAbsoluteProjectPath(AssetPath);
    const std::filesystem::path ContentRoot = std::filesystem::path(FPaths::ContentDir()).lexically_normal();
    if (!IsUnderDirectory(NormalizedPath, ContentRoot))
    {
        FNotificationManager::Get().AddNotification(
            "Asset Editor opens only .uasset files under Asset/Content. Use Import Source Asset first.", ENotificationType::Error, 5.0f);
        return false;
    }

    if (AssetEditorWindow.ActivateTabByAssetPath(NormalizedPath))
    {
        AssetEditorWindow.Show();
        if (EditorEngine)
        {
            EditorEngine->SetActiveEditorContext(EEditorContextType::AssetEditor);
        }
        return true;
    }

    std::unique_ptr<IAssetEditor> Editor = CreateEditorForAsset(Asset);
    if (!Editor)
    {
        NotifyUnsupportedAssetEditor(EditorEngine, NormalizedPath);
        return false;
    }

    Editor->Initialize(EditorEngine, Renderer);
    if (!Editor->OpenAsset(Asset, NormalizedPath))
    {
        Editor->Close();
        return false;
    }

    AssetEditorWindow.Initialize(EditorEngine, this);

    AssetEditorWindow.OpenEditorTab(std::move(Editor));
    AssetEditorWindow.Show();
    if (EditorEngine)
    {
        EditorEngine->SetActiveEditorContext(EEditorContextType::AssetEditor);
    }
    return true;
}

bool FAssetEditorManager::OpenAssetWithDialog(void *OwnerWindowHandle)
{
    const FString SelectedPath = FEditorFileUtils::OpenFileDialog({
        .Filter = L"Asset Files (*.uasset)\0*.uasset\0All Files (*.*)\0*.*\0",
        .Title = L"Open UAsset",
        .InitialDirectory = FPaths::ContentDir().c_str(),
        .OwnerWindowHandle = OwnerWindowHandle,
        .bFileMustExist = true,
        .bPathMustExist = true,
        .bPromptOverwrite = false,
        .bReturnRelativeToProjectRoot = false,
    });

    if (SelectedPath.empty())
    {
        return false;
    }

    return OpenAssetFromPath(std::filesystem::path(FPaths::ToWide(SelectedPath)));
}

bool FAssetEditorManager::ShowAssetEditorWindow()
{
    AssetEditorWindow.Initialize(EditorEngine, this);
    AssetEditorWindow.Show();
    if (EditorEngine)
    {
        EditorEngine->SetActiveEditorContext(EEditorContextType::AssetEditor);
    }
    return true;
}

bool FAssetEditorManager::CreateCameraModifierStackAsset()
{
    std::unique_ptr<FCameraModifierStackEditor> Editor = std::make_unique<FCameraModifierStackEditor>();
    Editor->Initialize(EditorEngine, Renderer);
    if (!Editor->CreateCameraShakeAsset())
    {
        Editor->Close();
        return false;
    }

    AssetEditorWindow.Initialize(EditorEngine, this);

    AssetEditorWindow.OpenEditorTab(std::move(Editor));
    AssetEditorWindow.Show();
    if (EditorEngine)
    {
        EditorEngine->SetActiveEditorContext(EEditorContextType::AssetEditor);
    }
    return true;
}

bool FAssetEditorManager::SaveActiveEditor() { return AssetEditorWindow.SaveActiveTab(); }

void FAssetEditorManager::CloseActiveEditor()
{
    AssetEditorWindow.CloseActiveTab();
    if (EditorEngine && !AssetEditorWindow.IsOpen())
    {
        EditorEngine->SetActiveEditorContext(EEditorContextType::LevelEditor);
    }
}

bool FAssetEditorManager::CloseAllEditors(bool bPromptForDirty, void *OwnerWindowHandle)
{
    const bool bClosed = AssetEditorWindow.CloseAllTabs(bPromptForDirty, OwnerWindowHandle);
    if (EditorEngine && !AssetEditorWindow.IsOpen())
    {
        EditorEngine->SetActiveEditorContext(EEditorContextType::LevelEditor);
    }
    return bClosed;
}

bool FAssetEditorManager::HasDirtyEditors() const { return AssetEditorWindow.HasDirtyTabs(); }

bool FAssetEditorManager::ConfirmCloseAllEditors(void *OwnerWindowHandle) const
{
    return AssetEditorWindow.ConfirmCloseAllTabs(OwnerWindowHandle);
}

bool FAssetEditorManager::IsCapturingInput() const { return AssetEditorWindow.IsCapturingInput(); }

FEditorViewportClient *FAssetEditorManager::GetActiveViewportClient() const { return AssetEditorWindow.GetActiveViewportClient(); }

void FAssetEditorManager::CollectViewportClients(TArray<FEditorViewportClient *> &OutClients) const
{
    AssetEditorWindow.CollectViewportClients(OutClients);
}

void FAssetEditorManager::ForceDeactivateAllViewportClients()
{
    AssetEditorWindow.ForceDeactivateAllViewportClients();
}

std::unique_ptr<IAssetEditor> FAssetEditorManager::CreateEditorForAsset(UObject *Asset) const
{
    if (Cast<UCameraModifierStackAssetData>(Asset))
    {
        return std::make_unique<FCameraModifierStackEditor>();
    }

    if (Cast<USkeletalMesh>(Asset))
    {
        return std::make_unique<FSkeletalMeshEditor>();
    }

    return nullptr;
}
