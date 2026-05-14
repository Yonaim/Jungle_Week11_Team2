#include "PCH/LunaticPCH.h"
#include "AssetEditor/SkeletalMesh/SkeletalMeshEditor.h"

#include "AssetEditor/SkeletalMesh/SkeletalMeshPreviewPoseController.h"
#include "Common/UI/Docking/DockLayoutUtils.h"
#include "Common/UI/Style/EditorUIStyle.h"
#include "Core/Notification.h"
#include "Component/SkeletalMeshComponent.h"
#include "EditorEngine.h"
#include "Engine/Asset/AssetFileSerializer.h"
#include "Engine/Mesh/SkeletalMesh.h"
#include "Materials/MaterialManager.h"
#include "Mesh/MeshAssetManager.h"
#include "Object/Object.h"
#include "Platform/Paths.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"

#include <string>
#include <utility>

namespace
{
uint32 GNextSkeletalMeshEditorId = 1;

FString ToLowerAsciiCopy(FString Value)
{
    for (char& Character : Value)
    {
        if (Character >= 'A' && Character <= 'Z')
        {
            Character = static_cast<char>(Character - 'A' + 'a');
        }
    }
    return Value;
}
}

void FSkeletalMeshEditor::Initialize(UEditorEngine *InEditorEngine, FRenderer *InRenderer)
{
    EditorEngine = InEditorEngine;
    Renderer = InRenderer;

    if (EditorInstanceId == 0)
    {
        EditorInstanceId = GNextSkeletalMeshEditorId++;
    }

    PreviewViewport.Initialize(EditorEngine ? EditorEngine->GetWindow() : nullptr, Renderer);
    if (!PoseController)
    {
        PoseController = std::make_shared<FSkeletalMeshPreviewPoseController>();
    }
    if (FSkeletalMeshPreviewViewportClient *PreviewClient = PreviewViewport.GetViewportClient())
    {
        PreviewClient->SetPoseController(PoseController);
    }
}

bool FSkeletalMeshEditor::OpenAsset(UObject *Asset, const std::filesystem::path &AssetPath)
{
    USkeletalMesh *SkeletalMesh = Cast<USkeletalMesh>(Asset);
    if (!SkeletalMesh)
    {
        return false;
    }

    Close();

    EditingAsset = SkeletalMesh;
    EditingAssetPath = AssetPath.lexically_normal();
    bDisablePreviewForCurrentAsset = ShouldDisablePreviewForAsset(EditingAssetPath);
    if (FSkeletalMesh *MeshData = EditingAsset->GetSkeletalMeshAsset())
    {
        if (MeshData->PathFileName.empty())
        {
            MeshData->PathFileName = FPaths::ToUtf8(EditingAssetPath.generic_wstring());
        }
        MeshData->BuildBoneHierarchyCache();
    }

    State = FSkeletalMeshEditorState{};
    SelectionManager.Reset();
    UndoStack.clear();
    RedoStack.clear();
    BuiltDockspaceId = 0;

    bPreviewPanelOpen = true;
    bSkeletonTreePanelOpen = true;
    bDetailsPanelOpen = true;
    bBoneDetailsPanelOpen = true;
    bPreviewerSettingsPanelOpen = !bDisablePreviewForCurrentAsset;

    bOpen = true;
    bDirty = false;

    FNotificationManager::Get().AddNotification("Opened asset: " + FPaths::ToUtf8(EditingAssetPath.filename().wstring()),
                                                ENotificationType::Success, 3.0f);
    return true;
}

void FSkeletalMeshEditor::Close()
{
    if (EditingAsset)
    {
        UObjectManager::Get().DestroyObject(EditingAsset);
        EditingAsset = nullptr;
    }

    EditingAssetPath.clear();
    State = FSkeletalMeshEditorState{};
    SelectionManager.Reset();
    UndoStack.clear();
    RedoStack.clear();
    BuiltDockspaceId = 0;

    bPreviewPanelOpen = true;
    bSkeletonTreePanelOpen = true;
    bDetailsPanelOpen = true;
    bBoneDetailsPanelOpen = true;
    bPreviewerSettingsPanelOpen = true;
    bDisablePreviewForCurrentAsset = false;

    bOpen = false;
    bDirty = false;
    bCapturingInput = false;
    bIsActiveTab = false;
}

bool FSkeletalMeshEditor::Save()
{
    if (!EditingAsset)
    {
        FNotificationManager::Get().AddNotification("No Skeletal Mesh asset is open.", ENotificationType::Info, 3.0f);
        return false;
    }

    if (EditingAssetPath.empty())
    {
        FNotificationManager::Get().AddNotification("Cannot save Skeletal Mesh: asset path is empty.", ENotificationType::Error, 4.0f);
        return false;
    }

    FString Error;
    if (!FAssetFileSerializer::SaveObjectToAssetFile(EditingAssetPath, EditingAsset, &Error))
    {
        FNotificationManager::Get().AddNotification(
            Error.empty() ? "Failed to save Skeletal Mesh asset." : Error,
            ENotificationType::Error,
            4.0f);
        return false;
    }

    FMeshAssetManager::MarkAssetCacheStale(FPaths::ToUtf8(EditingAssetPath.generic_wstring()));

    bDirty = false;
    FNotificationManager::Get().AddNotification(
        "Saved asset: " + FPaths::ToUtf8(EditingAssetPath.filename().wstring()),
        ENotificationType::Success,
        3.0f);
    return true;
}

void FSkeletalMeshEditor::Tick(float DeltaTime)
{
    if (!bOpen || !bIsActiveTab || !CanUsePreviewViewport())
    {
        PreviewViewport.DeactivateEditorContext();
        return;
    }

    PreviewViewport.Tick(DeltaTime);
}

void FSkeletalMeshEditor::RenderContent(float DeltaTime)
{
    RenderPanels(DeltaTime, 0);
}

void FSkeletalMeshEditor::InvalidateDockLayout()
{
    // Returning to an Asset Editor tab should always show the canonical default
    // Skeletal Mesh Editor layout. Re-open every default panel and force the
    // DockBuilder layout to be rebuilt on the next render.
    bPreviewPanelOpen = true;
    bSkeletonTreePanelOpen = true;
    bDetailsPanelOpen = true;
    bBoneDetailsPanelOpen = true;
    bPreviewerSettingsPanelOpen = true;
    BuiltDockspaceId = 0;
}

void FSkeletalMeshEditor::OnActivated()
{
    bIsActiveTab = true;
    bCapturingInput = false;
    if (bOpen && CanUsePreviewViewport())
    {
        PreviewViewport.BindEditorContext(State, &SelectionManager);
    }
}

void FSkeletalMeshEditor::OnDeactivated()
{
    if (FSkeletalMeshPreviewViewportClient* PreviewClient = PreviewViewport.GetViewportClient())
    {
        if (PreviewClient->IsEditorContextActive())
        {
            PreviewViewport.DeactivateEditorContext();
        }
    }

    bIsActiveTab = false;
    bCapturingInput = false;
}

void FSkeletalMeshEditor::RenderPanels(float DeltaTime, ImGuiID DockspaceId)
{
    if (!bOpen)
    {
        bCapturingInput = false;
        return;
    }

    if (DockspaceId != 0 && BuiltDockspaceId != DockspaceId)
    {
        BuildDefaultDockLayout(DockspaceId);
        BuiltDockspaceId = DockspaceId;
    }

    RenderPanelsInternal(DeltaTime, DockspaceId);

    FSkeletalMeshPreviewViewportClient *PreviewClient = PreviewViewport.GetViewportClient();
    bCapturingInput = bIsActiveTab && PreviewClient && (PreviewClient->IsHovered() || PreviewClient->IsActive());
}

FEditorViewportClient *FSkeletalMeshEditor::GetActiveViewportClient()
{
    if (!bOpen || !CanUsePreviewViewport())
    {
        return nullptr;
    }

    if (bIsActiveTab)
    {
        PreviewViewport.BindEditorContext(State, &SelectionManager);
    }

    return PreviewViewport.GetViewportClient();
}

void FSkeletalMeshEditor::CollectViewportClients(TArray<FEditorViewportClient *> &OutClients)
{
    if (!bOpen || !CanUsePreviewViewport())
    {
        return;
    }

    // Deactivation must still be able to find the preview viewport after the tab has
    // already been marked inactive. Only re-bind editor state while it is still active.
    if (bIsActiveTab)
    {
        PreviewViewport.BindEditorContext(State, &SelectionManager);
    }

    if (FSkeletalMeshPreviewViewportClient *Client = PreviewViewport.GetViewportClient())
    {
        OutClients.push_back(Client);
    }
}

void FSkeletalMeshEditor::BuildWindowMenu()
{
    if (ImGui::MenuItem("Preview Viewport", nullptr, bPreviewPanelOpen))
        bPreviewPanelOpen = !bPreviewPanelOpen;
    if (ImGui::MenuItem("Skeleton Tree", nullptr, bSkeletonTreePanelOpen))
        bSkeletonTreePanelOpen = !bSkeletonTreePanelOpen;
    if (ImGui::MenuItem("Asset Details", nullptr, bDetailsPanelOpen))
        bDetailsPanelOpen = !bDetailsPanelOpen;
    if (ImGui::MenuItem("Details", nullptr, bBoneDetailsPanelOpen))
        bBoneDetailsPanelOpen = !bBoneDetailsPanelOpen;
    if (ImGui::MenuItem("Previewer Settings", nullptr, bPreviewerSettingsPanelOpen))
        bPreviewerSettingsPanelOpen = !bPreviewerSettingsPanelOpen;

    if (ImGui::MenuItem("Reset Skeletal Mesh Editor Layout"))
    {
        bPreviewPanelOpen = true;
        bSkeletonTreePanelOpen = true;
        bDetailsPanelOpen = true;
        bBoneDetailsPanelOpen = true;
        bPreviewerSettingsPanelOpen = true;
        BuiltDockspaceId = 0;
    }
}

void FSkeletalMeshEditor::BuildCustomMenus()
{
    if (ImGui::BeginMenu("Mesh"))
    {
        ImGui::MenuItem("Reimport", nullptr, false, false);
        ImGui::MenuItem("Reset Preview", nullptr, false, EditingAsset != nullptr);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Skeleton"))
    {
        ImGui::MenuItem("Show Bones", nullptr, &State.bShowBones, EditingAsset != nullptr);
        ImGui::MenuItem("Pose Edit Mode", nullptr, &State.bEnablePoseEditMode, EditingAsset != nullptr);
        ImGui::MenuItem("Bone Gizmo", nullptr, false, false);
        ImGui::EndMenu();
    }
}

bool FSkeletalMeshEditor::CanUndo() const
{
    return !UndoStack.empty();
}

bool FSkeletalMeshEditor::CanRedo() const
{
    return !RedoStack.empty();
}

void FSkeletalMeshEditor::Undo()
{
    if (!CanUndo())
    {
        return;
    }

    FHistoryState CurrentState = CaptureHistoryState();
    FHistoryState PreviousState = UndoStack.back();
    UndoStack.pop_back();

    RedoStack.push_back(std::move(CurrentState));
    ApplyHistoryState(PreviousState);
    bDirty = true;
}

void FSkeletalMeshEditor::Redo()
{
    if (!CanRedo())
    {
        return;
    }

    FHistoryState CurrentState = CaptureHistoryState();
    FHistoryState NextState = RedoStack.back();
    RedoStack.pop_back();

    UndoStack.push_back(std::move(CurrentState));
    ApplyHistoryState(NextState);
    bDirty = true;
}

FSkeletalMeshEditor::FHistoryState FSkeletalMeshEditor::CaptureHistoryState() const
{
    FHistoryState StateSnapshot;
    if (!EditingAsset)
    {
        return StateSnapshot;
    }

    const TArray<FStaticMaterial> &Materials = EditingAsset->GetStaticMaterials();
    StateSnapshot.MaterialPaths.reserve(Materials.size());
    for (const FStaticMaterial &Material : Materials)
    {
        StateSnapshot.MaterialPaths.push_back(Material.MaterialInterface
            ? Material.MaterialInterface->GetAssetPathFileName()
            : FString("None"));
    }
    return StateSnapshot;
}

void FSkeletalMeshEditor::ApplyHistoryState(const FHistoryState &HistoryState)
{
    if (!EditingAsset)
    {
        return;
    }

    TArray<FStaticMaterial> &Materials = EditingAsset->GetStaticMaterialsMutable();
    const int32 ApplyCount = (std::min)(static_cast<int32>(Materials.size()), static_cast<int32>(HistoryState.MaterialPaths.size()));
    USkeletalMeshComponent *PreviewComponent = GetPreviewComponent();

    for (int32 MaterialIndex = 0; MaterialIndex < ApplyCount; ++MaterialIndex)
    {
        const FString &MaterialPath = HistoryState.MaterialPaths[MaterialIndex];
        UMaterial *Material = nullptr;
        if (!MaterialPath.empty() && MaterialPath != "None")
        {
            Material = FMaterialManager::Get().GetOrCreateMaterial(MaterialPath);
        }

        Materials[MaterialIndex].MaterialInterface = Material;
        if (PreviewComponent)
        {
            PreviewComponent->SetMaterial(MaterialIndex, Material);
        }
    }
}

bool FSkeletalMeshEditor::AreHistoryStatesEqual(const FHistoryState &A, const FHistoryState &B) const
{
    if (A.MaterialPaths.size() != B.MaterialPaths.size())
    {
        return false;
    }

    for (size_t Index = 0; Index < A.MaterialPaths.size(); ++Index)
    {
        if (A.MaterialPaths[Index] != B.MaterialPaths[Index])
        {
            return false;
        }
    }
    return true;
}

void FSkeletalMeshEditor::PushUndoStateIfChanged(const FHistoryState &BeforeState, const FHistoryState &AfterState)
{
    if (AreHistoryStatesEqual(BeforeState, AfterState))
    {
        return;
    }

    UndoStack.push_back(BeforeState);
    RedoStack.clear();
    bDirty = true;
}

USkeletalMeshComponent *FSkeletalMeshEditor::GetPreviewComponent() const
{
    if (!CanUsePreviewViewport())
    {
        return nullptr;
    }

    FSkeletalMeshPreviewViewportClient *PreviewClient = const_cast<FSkeletalMeshPreviewViewport &>(PreviewViewport).GetViewportClient();
    return PreviewClient ? PreviewClient->GetPreviewComponent() : nullptr;
}

bool FSkeletalMeshEditor::ShouldDisablePreviewForAsset(const std::filesystem::path& AssetPath) const
{
    (void)AssetPath;
    return false;
}

bool FSkeletalMeshEditor::CanUsePreviewViewport() const
{
    return bPreviewPanelOpen && !bDisablePreviewForCurrentAsset;
}

void FSkeletalMeshEditor::RenderPreviewDisabledPanel(const FPanelDesc& Desc) const
{
    if (!FPanel::Begin(Desc))
    {
        FPanel::End();
        return;
    }

    ImGui::TextWrapped("Preview viewport is temporarily disabled for this asset to avoid editor crashes.");
    ImGui::Spacing();
    ImGui::TextWrapped("You can still inspect the asset, change material slots, and save the .uasset safely.");
    if (!EditingAssetPath.empty())
    {
        ImGui::Spacing();
        ImGui::TextDisabled("%s", FPaths::ToUtf8(EditingAssetPath.filename().wstring()).c_str());
    }

    FPanel::End();
}

std::string FSkeletalMeshEditor::MakePanelStableId(const char *PanelName) const
{
    return std::string("SkeletalMeshEditor_") + std::to_string(EditorInstanceId) + "_" + PanelName;
}

FPanelDesc FSkeletalMeshEditor::MakePanelDesc(const char *DisplayName, const char *StableName, const char *IconKey,
                                                    ImGuiWindowFlags Flags) const
{
    FPanelDesc Desc;
    Desc.DisplayName = DisplayName;
    Desc.IconKey = IconKey;
    Desc.WindowFlags = Flags;
    Desc.bClosable = true;
    Desc.bApplyContentTopInset = true;
    Desc.bApplySideInset = true;
    Desc.bApplyBottomInset = true;

    (void)StableName;
    return Desc;
}

void FSkeletalMeshEditor::BuildDefaultDockLayout(ImGuiID DockspaceId)
{
    if (DockspaceId == 0)
    {
        return;
    }

    const std::string SkeletonId = MakePanelStableId("SkeletonTree");
    const std::string PreviewId = MakePanelStableId("PreviewViewport");
    const std::string DetailsId = MakePanelStableId("AssetDetails");
    const std::string BoneDetailsId = MakePanelStableId("Details");
    const std::string PreviewerSettingsId = MakePanelStableId("PreviewerSettings");

    FPanelDesc SkeletonDesc = MakePanelDesc("Skeleton Tree", "SkeletonTree", "Editor.Icon.Panel.SkeletonTree");
    SkeletonDesc.StableId = SkeletonId.c_str();
    SkeletonDesc.bOpen = &bSkeletonTreePanelOpen;

    const std::string PreviewTitle = std::string("Viewport ") + std::to_string(EditorInstanceId);
    FPanelDesc PreviewDesc = MakePanelDesc(PreviewTitle.c_str(), "PreviewViewport", "Editor.Icon.Panel.Viewport");
    PreviewDesc.StableId = PreviewId.c_str();

    FPanelDesc DetailsDesc = MakePanelDesc("Asset Details", "AssetDetails", "Editor.Icon.SkeletalMesh");
    DetailsDesc.StableId = DetailsId.c_str();

    FPanelDesc BoneDetailsDesc = MakePanelDesc("Details", "Details", "Editor.Icon.Panel.Details");
    BoneDetailsDesc.StableId = BoneDetailsId.c_str();

    FPanelDesc PreviewerSettingsDesc = MakePanelDesc("Previewer Settings", "PreviewerSettings", "Editor.Icon.Panel.PreviewerSettings");
    PreviewerSettingsDesc.StableId = PreviewerSettingsId.c_str();

    FAssetPreviewDockLayoutDesc LayoutDesc;
    LayoutDesc.CenterWindow = FPanel::MakeTitle(PreviewDesc);
    LayoutDesc.RightTopWindow = FPanel::MakeTitle(SkeletonDesc);
    LayoutDesc.RightBottomWindow = FPanel::MakeTitle(DetailsDesc);
    LayoutDesc.RightBottomSecondWindow = FPanel::MakeTitle(BoneDetailsDesc);
    LayoutDesc.RightBottomSideWindow = FPanel::MakeTitle(PreviewerSettingsDesc);

    FDockLayoutUtils::DockAssetPreviewLayout(DockspaceId, LayoutDesc);
}

void FSkeletalMeshEditor::RenderPanelsInternal(float DeltaTime, ImGuiID DockspaceId)
{
    (void)DockspaceId;

    const std::string SkeletonId = MakePanelStableId("SkeletonTree");
    const std::string PreviewId = MakePanelStableId("PreviewViewport");
    const std::string DetailsId = MakePanelStableId("AssetDetails");
    const std::string BoneDetailsId = MakePanelStableId("Details");
    const std::string PreviewerSettingsId = MakePanelStableId("PreviewerSettings");

    FPanelDesc SkeletonDesc = MakePanelDesc("Skeleton Tree", "SkeletonTree", "Editor.Icon.Panel.SkeletonTree");
    SkeletonDesc.StableId = SkeletonId.c_str();
    SkeletonDesc.bOpen = &bSkeletonTreePanelOpen;

    const std::string PreviewTitle = std::string("Viewport ") + std::to_string(EditorInstanceId);
    FPanelDesc PreviewDesc = MakePanelDesc(PreviewTitle.c_str(), "PreviewViewport", "Editor.Icon.Panel.Viewport",
                                                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
                                                     ImGuiWindowFlags_NoScrollWithMouse);
    PreviewDesc.StableId = PreviewId.c_str();
    PreviewDesc.bOpen = &bPreviewPanelOpen;
    PreviewDesc.bApplySideInset = false;
    PreviewDesc.bApplyBottomInset = false;

    FPanelDesc DetailsDesc = MakePanelDesc("Asset Details", "AssetDetails", "Editor.Icon.SkeletalMesh");
    DetailsDesc.StableId = DetailsId.c_str();
    DetailsDesc.bOpen = &bDetailsPanelOpen;

    FPanelDesc BoneDetailsDesc = MakePanelDesc("Details", "Details", "Editor.Icon.Panel.Details");
    BoneDetailsDesc.StableId = BoneDetailsId.c_str();
    BoneDetailsDesc.bOpen = &bBoneDetailsPanelOpen;

    FPanelDesc PreviewerSettingsDesc = MakePanelDesc("Previewer Settings", "PreviewerSettings", "Editor.Icon.Panel.PreviewerSettings");
    PreviewerSettingsDesc.StableId = PreviewerSettingsId.c_str();
    PreviewerSettingsDesc.bOpen = &bPreviewerSettingsPanelOpen;

    if (bPreviewPanelOpen)
    {
        if (bDisablePreviewForCurrentAsset)
        {
            RenderPreviewDisabledPanel(PreviewDesc);
        }
        else
        {
            if (FSkeletalMeshPreviewViewportClient *PreviewClient = PreviewViewport.GetViewportClient())
            {
                PreviewClient->SetPoseController(PoseController);
            }
            PreviewViewport.Render(EditingAsset, State, &SelectionManager, &Toolbar, DeltaTime, PreviewDesc);
        }
    }
    if (bSkeletonTreePanelOpen)
    {
        SkeletonTreePanel.Render(EditingAsset, State, SelectionManager, SkeletonDesc);
    }
    if (bDetailsPanelOpen)
    {
        USkeletalMeshComponent *PreviewComponent = GetPreviewComponent();
        const FHistoryState BeforeState = CaptureHistoryState();

        AssetDetailsPanel.RenderSkeletalMesh(EditingAsset, PreviewComponent, EditingAssetPath, State,
                                             SelectionManager, DetailsDesc);

        const FHistoryState AfterState = CaptureHistoryState();
        PushUndoStateIfChanged(BeforeState, AfterState);
    }
    if (bBoneDetailsPanelOpen)
    {
        USkeletalMeshComponent *PreviewComponent = nullptr;
        if (!bDisablePreviewForCurrentAsset)
        {
            if (FSkeletalMeshPreviewViewportClient *PreviewClient = PreviewViewport.GetViewportClient())
            {
                PreviewComponent = PreviewClient->GetPreviewComponent();
            }
        }
        DetailsPanel.Render(EditingAsset, PreviewComponent, PoseController.get(), State, SelectionManager, BoneDetailsDesc);
    }
    if (bPreviewerSettingsPanelOpen)
    {
        RenderPreviewerSettingsPanel(PreviewerSettingsDesc);
    }
}

void FSkeletalMeshEditor::RenderPreviewerSettingsPanel(const FPanelDesc& Desc)
{
    if (!FPanel::Begin(Desc))
    {
        FPanel::End();
        return;
    }

    ImGui::TextDisabled("Lighting");
    ImGui::Spacing();
    ImGui::Checkbox("Enable Preview Lighting", &State.bPreviewLighting);
    ImGui::BeginDisabled(!State.bPreviewLighting);

    if (FEditorUIStyle::BeginDetailsSection("Ambient Light"))
    {
        ImGui::SliderFloat("Ambient Intensity", &State.PreviewAmbientLightIntensity, 0.0f, 2.0f, "%.2f");
        ImGui::ColorEdit3("Ambient Color", &State.PreviewAmbientLightColor.X);
    }

    if (FEditorUIStyle::BeginDetailsSection("Directional Light"))
    {
        ImGui::SliderFloat("Directional Intensity", &State.PreviewDirectionalLightIntensity, 0.0f, 8.0f, "%.2f");
        ImGui::ColorEdit3("Directional Color", &State.PreviewDirectionalLightColor.X);
        ImGui::SliderFloat("Light Yaw", &State.PreviewLightYaw, -180.0f, 180.0f, "%.1f deg");
        ImGui::SliderFloat("Light Pitch", &State.PreviewLightPitch, -89.0f, 89.0f, "%.1f deg");
        ImGui::TextDisabled("Direction is applied to the preview scene every frame.");
    }

    ImGui::EndDisabled();
    ImGui::Spacing();
    if (ImGui::Button("Reset Lighting"))
    {
        State.bPreviewLighting = true;
        State.PreviewDirectionalLightIntensity = 1.0f;
        State.PreviewAmbientLightIntensity = 0.25f;
        State.PreviewDirectionalLightColor = FVector4(1.0f, 0.96f, 0.88f, 1.0f);
        State.PreviewAmbientLightColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
        State.PreviewLightYaw = 45.0f;
        State.PreviewLightPitch = -35.0f;
    }

    FPanel::End();
}
