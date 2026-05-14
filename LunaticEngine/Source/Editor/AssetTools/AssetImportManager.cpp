#include "PCH/LunaticPCH.h"
#include "AssetTools/AssetImportManager.h"

#include "Common/File/EditorFileUtils.h"
#include "Core/Log.h"
#include "Core/Notification.h"
#include "EditorEngine.h"
#include "Engine/Asset/AssetFileSerializer.h"
#include "Engine/Core/SimpleJsonWrapper.h"
#include "Engine/Platform/Paths.h"
#include "Materials/MaterialManager.h"
#include "Mesh/FbxImporter.h"
#include "Mesh/MeshAssetManager.h"
#include "Mesh/ObjImporter.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshCommon.h"
#include "Mesh/StaticMesh.h"
#include "Mesh/StaticMeshCommon.h"
#include "Object/ObjectFactory.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Texture/Texture2D.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <functional>
#include <vector>
#include <cwctype>

namespace
{
    std::wstring ToLowerExtension(const std::filesystem::path& Path)
    {
        std::wstring Ext = Path.extension().wstring();
        std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);
        return Ext;
    }

    std::filesystem::path ResolveSourcePathOnDisk(const FString& SourcePath)
    {
        return std::filesystem::path(FPaths::ResolvePathToDisk(SourcePath)).lexically_normal();
    }

    FString ToUtf8GenericPath(const std::filesystem::path& Path)
    {
        return FPaths::ToUtf8(Path.lexically_normal().generic_wstring());
    }

    FString SanitizeAssetName(FString Name)
    {
        if (Name.empty())
        {
            return "Imported";
        }

        for (char& Character : Name)
        {
            const bool bAlphaNum = (Character >= 'a' && Character <= 'z')
                || (Character >= 'A' && Character <= 'Z')
                || (Character >= '0' && Character <= '9');
            if (!bAlphaNum && Character != '_' && Character != '-')
            {
                Character = '_';
            }
        }
        return Name;
    }

    void EnsureUniquePath(std::filesystem::path& InOutPath)
    {
        if (!std::filesystem::exists(InOutPath))
        {
            return;
        }

        const std::filesystem::path Parent = InOutPath.parent_path();
        const std::wstring Stem = InOutPath.stem().wstring();
        const std::wstring Extension = InOutPath.extension().wstring();

        int32 Suffix = 1;
        do
        {
            InOutPath = Parent / (Stem + L"_" + std::to_wstring(Suffix++) + Extension);
        } while (std::filesystem::exists(InOutPath));
    }

    const char* GetAssetClassIdName(EAssetClassId ClassId)
    {
        switch (ClassId)
        {
        case EAssetClassId::StaticMesh:   return "UStaticMesh";
        case EAssetClassId::SkeletalMesh: return "USkeletalMesh";
        case EAssetClassId::Material:     return "UMaterial";
        case EAssetClassId::Texture:      return "UTexture2D";
        case EAssetClassId::PoseAsset:    return "USkeletonPoseAsset";
        default:                          return "UObject";
        }
    }

    bool CopyFileWithProgress(const std::filesystem::path& SourcePath,
                              const std::filesystem::path& DestinationPath,
                              const std::function<void(uint64, uint64)>& ProgressCallback,
                              FString* OutErrorMessage)
    {
        std::ifstream Input(SourcePath, std::ios::binary);
        if (!Input.is_open())
        {
            if (OutErrorMessage)
            {
                *OutErrorMessage = "Import failed: could not open source .uasset.";
            }
            return false;
        }

        std::ofstream Output(DestinationPath, std::ios::binary | std::ios::trunc);
        if (!Output.is_open())
        {
            if (OutErrorMessage)
            {
                *OutErrorMessage = "Import failed: could not create destination .uasset.";
            }
            return false;
        }

        const uint64 TotalBytes = static_cast<uint64>(std::filesystem::file_size(SourcePath));
        std::vector<char> Buffer(4 * 1024 * 1024);
        uint64 CopiedBytes = 0;

        if (ProgressCallback)
        {
            ProgressCallback(0, TotalBytes);
        }

        while (Input)
        {
            Input.read(Buffer.data(), static_cast<std::streamsize>(Buffer.size()));
            const std::streamsize BytesRead = Input.gcount();
            if (BytesRead <= 0)
            {
                break;
            }

            Output.write(Buffer.data(), BytesRead);
            if (!Output.good())
            {
                if (OutErrorMessage)
                {
                    *OutErrorMessage = "Import failed: write error while copying .uasset.";
                }
                return false;
            }

            CopiedBytes += static_cast<uint64>(BytesRead);
            if (ProgressCallback)
            {
                ProgressCallback(CopiedBytes, TotalBytes);
            }
        }

        if (!Input.eof() && Input.fail())
        {
            if (OutErrorMessage)
            {
                *OutErrorMessage = "Import failed: read error while copying .uasset.";
            }
            return false;
        }

        return true;
    }
}

void FAssetImportManager::Init(UEditorEngine* InEditorEngine)
{
    EditorEngine = InEditorEngine;
}

void FAssetImportManager::Shutdown()
{
    {
        std::lock_guard<std::mutex> Lock(AsyncMutex);
        PendingAsyncImports.clear();
    }

    if (AsyncImportThread.joinable())
    {
        AsyncImportThread.join();
    }
    bAsyncImportRunning = false;
    EditorEngine = nullptr;
}

void FAssetImportManager::Tick()
{
    FinalizeAsyncImportIfReady();
    StartNextAsyncImportIfIdle();
}

bool FAssetImportManager::ImportAssetWithDialog()
{
    if (!EditorEngine)
    {
        return false;
    }

    const FString SelectedPath = FEditorFileUtils::OpenFileDialog({
        .Filter = L"Importable Source Assets (*.fbx;*.obj;*.mtl;*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.dds;*.uasset)\0*.fbx;*.obj;*.mtl;*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.dds;*.uasset\0All Files (*.*)\0*.*\0",
        .Title = L"Import Source Asset",
        .InitialDirectory = FPaths::AssetDir().c_str(),
        .OwnerWindowHandle = EditorEngine->GetWindow() ? EditorEngine->GetWindow()->GetHWND() : nullptr,
        .bFileMustExist = true,
        .bPathMustExist = true,
        .bPromptOverwrite = false,
        .bReturnRelativeToProjectRoot = false,
    });

    if (SelectedPath.empty())
    {
        UE_LOG_CATEGORY(AssetImport, Debug, "[Import] Import dialog canceled.");
        return false;
    }

    return QueueImportAssetFromPath(SelectedPath);
}

bool FAssetImportManager::ImportAssetFromPath(const FString& SourcePath, FString* OutImportedAssetPath)
{
    const std::filesystem::path SourceAbsolute = ResolveSourcePathOnDisk(SourcePath);
    if (SourcePath.empty() || !std::filesystem::exists(SourceAbsolute))
    {
        UE_LOG_CATEGORY(AssetImport, Error, "[Import] Source file not found: %s", SourcePath.c_str());
        FNotificationManager::Get().AddNotification("Import failed: source file not found.", ENotificationType::Error, 5.0f);
        return false;
    }

    const FString SourceFileNameForToast = FPaths::ToUtf8(SourceAbsolute.filename().wstring());
    FNotificationManager::Get().AddNotification("Import 0%: preparing " + SourceFileNameForToast, ENotificationType::Info, 2.0f);
    UE_LOG_CATEGORY(AssetImport, Info, "[Import] Begin: source=%s", ToUtf8GenericPath(SourceAbsolute).c_str());

    const std::wstring Ext = ToLowerExtension(SourceAbsolute);
    FNotificationManager::Get().AddNotification("Import 30%: converting source asset", ENotificationType::Info, 2.0f);
    bool bResult = false;

    if (Ext == L".fbx" || Ext == L".obj")
    {
        bResult = ImportMeshSource(SourcePath, OutImportedAssetPath);
    }
    else if (Ext == L".mtl")
    {
        bResult = ImportMtlSource(SourcePath, OutImportedAssetPath);
    }
    else if (Ext == L".uasset")
    {
        bResult = ImportExistingUAsset(SourcePath, OutImportedAssetPath);
    }
    else if (UTexture2D::IsSupportedTextureExtension(SourceAbsolute))
    {
        bResult = ImportTextureSource(SourcePath, OutImportedAssetPath);
    }
    else
    {
        UE_LOG_CATEGORY(AssetImport, Warning, "[Import] Unsupported source type: %s", ToUtf8GenericPath(SourceAbsolute).c_str());
        FNotificationManager::Get().AddNotification("Import failed: unsupported source file type.", ENotificationType::Error, 5.0f);
        return false;
    }

    if (!bResult)
    {
        UE_LOG_CATEGORY(AssetImport, Error, "[Import] Failed: source=%s", ToUtf8GenericPath(SourceAbsolute).c_str());
        return false;
    }

    FNotificationManager::Get().AddNotification("Import 70%: writing .uasset and refreshing browser", ENotificationType::Info, 2.0f);

    FMeshAssetManager::ScanMeshAssets();
    FMeshAssetManager::ScanMeshSourceFiles();
    FMaterialManager::Get().ScanMaterialAssets();
    UTexture2D::ScanTextureAssets();
    if (EditorEngine)
    {
        EditorEngine->RefreshContentBrowser();
    }

    const FString ImportedPath = OutImportedAssetPath ? *OutImportedAssetPath : FString();
    const FString SourceFileName = FPaths::ToUtf8(SourceAbsolute.filename().wstring());
    const FString ImportedFileName = !ImportedPath.empty()
        ? FPaths::ToUtf8(std::filesystem::path(FPaths::ToWide(ImportedPath)).filename().wstring())
        : FString("asset");
    const FString Message = !ImportedPath.empty()
        ? "Imported " + SourceFileName + " as " + ImportedFileName
        : "Imported " + SourceFileName;
    FNotificationManager::Get().AddNotification(Message, ENotificationType::Success, 3.0f);
    UE_LOG_CATEGORY(AssetImport, Info, "[Import] %s | source=%s target=%s", Message.c_str(), ToUtf8GenericPath(SourceAbsolute).c_str(), ImportedPath.c_str());

    if (EditorEngine && !ImportedPath.empty())
    {
        // Import는 .uasset 생성 및 Content Browser 선택까지만 수행한다.
        // Asset Editor는 사용자가 생성된 .uasset을 직접 열 때만 열린다.
        EditorEngine->SelectContentBrowserPath(ImportedPath);
    }

    return true;
}

bool FAssetImportManager::QueueImportAssetFromPath(const FString& SourcePath)
{
    const std::filesystem::path SourceAbsolute = ResolveSourcePathOnDisk(SourcePath);
    if (SourcePath.empty() || !std::filesystem::exists(SourceAbsolute))
    {
        UE_LOG_CATEGORY(AssetImport, Error, "[Import] Source file not found: %s", SourcePath.c_str());
        FNotificationManager::Get().AddNotification("Import failed: source file not found.", ENotificationType::Error, 5.0f);
        return false;
    }

    if (!ShouldImportAsync(SourcePath))
    {
        FString ImportedAssetPath;
        return ImportAssetFromPath(SourcePath, &ImportedAssetPath);
    }

    const FString SourceFileName = FPaths::ToUtf8(SourceAbsolute.filename().wstring());
    {
        std::lock_guard<std::mutex> Lock(AsyncMutex);
        PendingAsyncImports.push_back({ SourcePath });
    }

    FNotificationManager::Get().AddNotification("Queued import: " + SourceFileName, ENotificationType::Info, 2.0f);
    StartNextAsyncImportIfIdle();
    return true;
}

bool FAssetImportManager::ImportMaterialWithDialog()
{
    return ImportAssetWithDialog();
}

bool FAssetImportManager::ImportTextureWithDialog()
{
    return ImportAssetWithDialog();
}

bool FAssetImportManager::ImportMeshSource(const FString& SourcePath, FString* OutImportedAssetPath)
{
    const std::filesystem::path SourceDiskPath = ResolveSourcePathOnDisk(SourcePath);
    const FString SourceDiskPathString = ToUtf8GenericPath(SourceDiskPath);
    const std::wstring Ext = ToLowerExtension(SourceDiskPath);
    const bool bSkeletalMesh = Ext == L".fbx" && FMeshAssetManager::IsFbxSkeletalMesh(SourceDiskPathString);
    const wchar_t* Prefix = bSkeletalMesh ? L"SK_" : L"SM_";
    std::filesystem::path DestinationPath = MakeDestinationAssetPath(SourceDiskPathString, L"Meshes", Prefix);
    EnsureUniquePath(DestinationPath);

    UE_LOG_CATEGORY(AssetImport, Info, "[Import] Source classified as %s: source=%s target=%s",
                    bSkeletalMesh ? "USkeletalMesh" : "UStaticMesh",
                    SourceDiskPathString.c_str(), ToUtf8GenericPath(DestinationPath).c_str());

    if (bSkeletalMesh)
    {
        USkeletalMesh* Mesh = UObjectManager::Get().CreateObject<USkeletalMesh>();
        FSkeletalMesh* MeshData = new FSkeletalMesh();
        TArray<FStaticMaterial> Materials;

        if (!FFbxSkeletalMeshImporter::Import(SourceDiskPathString, *MeshData, Materials))
        {
            delete MeshData;
            UObjectManager::Get().DestroyObject(Mesh);
            UE_LOG_CATEGORY(AssetImport, Error, "[Import] Skeletal mesh conversion failed: %s", SourceDiskPathString.c_str());
            FNotificationManager::Get().AddNotification("Import failed: skeletal FBX conversion failed.", ENotificationType::Error, 5.0f);
            return false;
        }

        MeshData->PathFileName = MakeProjectRelativePath(DestinationPath);
        Mesh->SetFName(FName(FPaths::ToUtf8(DestinationPath.stem().wstring())));
        Mesh->SetStaticMaterials(std::move(Materials));
        Mesh->SetSkeletalMeshAsset(MeshData);
        return SaveImportedObject(DestinationPath, Mesh, OutImportedAssetPath);
    }

    UStaticMesh* Mesh = UObjectManager::Get().CreateObject<UStaticMesh>();
    FStaticMesh* MeshData = new FStaticMesh();
    TArray<FStaticMaterial> Materials;
    FImportOptions Options = FImportOptions::Default();

    const bool bImported = Ext == L".fbx"
        ? FFbxStaticMeshImporter::Import(SourceDiskPathString, *MeshData, Materials)
        : FObjImporter::Import(SourceDiskPathString, Options, *MeshData, Materials);

    if (!bImported)
    {
        delete MeshData;
        UObjectManager::Get().DestroyObject(Mesh);
        UE_LOG_CATEGORY(AssetImport, Error, "[Import] Static mesh conversion failed: %s", SourceDiskPathString.c_str());
        FNotificationManager::Get().AddNotification("Import failed: static mesh conversion failed.", ENotificationType::Error, 5.0f);
        return false;
    }

    MeshData->PathFileName = MakeProjectRelativePath(DestinationPath);
    Mesh->SetFName(FName(FPaths::ToUtf8(DestinationPath.stem().wstring())));
    Mesh->SetStaticMaterials(std::move(Materials));
    Mesh->SetStaticMeshAsset(MeshData);
    return SaveImportedObject(DestinationPath, Mesh, OutImportedAssetPath);
}

bool FAssetImportManager::ImportMtlSource(const FString& SourcePath, FString* OutImportedAssetPath)
{
    const FString SourceDiskPathString = ToUtf8GenericPath(ResolveSourcePathOnDisk(SourcePath));
    TArray<FString> GeneratedMaterialAssetPaths;
    if (!FObjImporter::ImportMtl(SourceDiskPathString, &GeneratedMaterialAssetPaths))
    {
        UE_LOG_CATEGORY(AssetImport, Error, "[Import] MTL conversion failed: %s", SourceDiskPathString.c_str());
        FNotificationManager::Get().AddNotification("Import failed: MTL conversion failed.", ENotificationType::Error, 5.0f);
        return false;
    }

    if (OutImportedAssetPath)
    {
        *OutImportedAssetPath = GeneratedMaterialAssetPaths.empty() ? FString() : GeneratedMaterialAssetPaths.front();
    }
    UE_LOG_CATEGORY(AssetImport, Info, "[Import] MTL generated %zu material asset(s).", GeneratedMaterialAssetPaths.size());
    return true;
}

bool FAssetImportManager::ImportTextureSource(const FString& SourcePath, FString* OutImportedAssetPath)
{
    const std::filesystem::path SourceDiskPath = ResolveSourcePathOnDisk(SourcePath);
    const FString SourceDiskPathString = ToUtf8GenericPath(SourceDiskPath);
    const FString ImportedAssetPath = UTexture2D::ImportTextureAsset(SourceDiskPathString);
    if (ImportedAssetPath.empty())
    {
        UE_LOG_CATEGORY(AssetImport, Error, "[Import] Texture conversion failed: %s", SourceDiskPathString.c_str());
        FNotificationManager::Get().AddNotification("Import failed: texture conversion failed.", ENotificationType::Error, 5.0f);
        return false;
    }

    if (OutImportedAssetPath)
    {
        *OutImportedAssetPath = ImportedAssetPath;
    }

    UE_LOG_CATEGORY(AssetImport, Info, "[Import] Source classified as UTexture2D: source=%s target=%s", SourceDiskPathString.c_str(), ImportedAssetPath.c_str());
    return true;
}

bool FAssetImportManager::ImportExistingUAsset(const FString& SourcePath, FString* OutImportedAssetPath)
{
    FString ErrorMessage;
    const bool bImported = ImportExistingUAssetWithProgress(SourcePath, OutImportedAssetPath, &ErrorMessage);
    if (!bImported)
    {
        const std::filesystem::path SourceAbsolute = ResolveSourcePathOnDisk(SourcePath);
        UE_LOG_CATEGORY(AssetImport, Error, "[Import] Existing .uasset import failed: %s", ToUtf8GenericPath(SourceAbsolute).c_str());
        FNotificationManager::Get().AddNotification(ErrorMessage.empty() ? "Import failed: invalid .uasset file." : ErrorMessage,
                                                    ENotificationType::Error,
                                                    5.0f);
    }
    return bImported;
}

bool FAssetImportManager::SaveImportedObject(const std::filesystem::path& DestinationPath, UObject* Object, FString* OutImportedAssetPath)
{
    if (!Object)
    {
        UE_LOG_CATEGORY(AssetImport, Error, "[Import] Save failed: null UObject. target=%s", ToUtf8GenericPath(DestinationPath).c_str());
        return false;
    }

    std::filesystem::create_directories(DestinationPath.parent_path());

    FString Error;
    const bool bSaved = FAssetFileSerializer::SaveObjectToAssetFile(DestinationPath, Object, &Error);
    if (!bSaved)
    {
        UE_LOG_CATEGORY(AssetImport, Error, "[Import] Save .uasset failed: target=%s error=%s", ToUtf8GenericPath(DestinationPath).c_str(), Error.c_str());
        FNotificationManager::Get().AddNotification(Error.empty() ? "Import failed: could not save .uasset." : Error,
                                                    ENotificationType::Error, 5.0f);
        UObjectManager::Get().DestroyObject(Object);
        return false;
    }

    FString RelativePath = MakeProjectRelativePath(DestinationPath);
    if (OutImportedAssetPath)
    {
        *OutImportedAssetPath = RelativePath;
    }

    FAssetFileHeader Header;
    const char* ClassName = "UObject";
    if (FAssetFileSerializer::ReadAssetHeader(DestinationPath, Header))
    {
        ClassName = GetAssetClassIdName(Header.ClassId);
    }
    UE_LOG_CATEGORY(AssetImport, Info, "[AssetSave] Saved %s .uasset: %s", ClassName, RelativePath.c_str());
    return true;
}

std::filesystem::path FAssetImportManager::MakeDestinationAssetPath(const FString& SourcePath, const wchar_t* CategoryDir, const wchar_t* Prefix) const
{
    const std::wstring Category(CategoryDir ? CategoryDir : L"");
    const std::wstring PrefixString(Prefix ? Prefix : L"");

    if (Category == L"Meshes")
    {
        const FString AssetPath = PrefixString == L"SK_"
            ? FMeshAssetManager::GetSkeletalMeshAssetPath(SourcePath)
            : FMeshAssetManager::GetStaticMeshAssetPath(SourcePath);
        return std::filesystem::path(FPaths::ResolvePathToDisk(AssetPath)).lexically_normal();
    }

    const std::filesystem::path Source(FPaths::ToWide(SourcePath));
    const FString SourceStem = SanitizeAssetName(FPaths::ToUtf8(Source.stem().wstring()));

    std::filesystem::path DestinationDirectory = std::filesystem::path(FPaths::ContentDir()) / CategoryDir;
    return DestinationDirectory / (std::wstring(Prefix) + FPaths::ToWide(SourceStem) + L".uasset");
}

FString FAssetImportManager::MakeProjectRelativePath(const std::filesystem::path& Path) const
{
    const std::filesystem::path ProjectRoot(FPaths::RootDir());
    const std::filesystem::path Normalized = Path.lexically_normal();
    const std::filesystem::path Relative = Normalized.lexically_relative(ProjectRoot);
    if (!Relative.empty() && Relative.native().find(L"..") != 0)
    {
        return FPaths::NormalizePath(FPaths::ToUtf8(Relative.generic_wstring()));
    }
    return FPaths::NormalizePath(FPaths::ToUtf8(Normalized.generic_wstring()));
}

bool FAssetImportManager::ShouldImportAsync(const FString& SourcePath) const
{
    return ToLowerExtension(ResolveSourcePathOnDisk(SourcePath)) == L".uasset";
}

void FAssetImportManager::StartNextAsyncImportIfIdle()
{
    if (bAsyncImportRunning)
    {
        return;
    }

    FPendingAsyncImport PendingImport;
    {
        std::lock_guard<std::mutex> Lock(AsyncMutex);
        if (PendingAsyncImports.empty())
        {
            return;
        }

        PendingImport = PendingAsyncImports.front();
        PendingAsyncImports.pop_front();
        AsyncResult = {};
    }

    if (AsyncImportThread.joinable())
    {
        AsyncImportThread.join();
    }

    bAsyncImportRunning = true;
    AsyncImportThread = std::thread(&FAssetImportManager::StartAsyncImportWorker, this, PendingImport.SourcePath);
}

void FAssetImportManager::FinalizeAsyncImportIfReady()
{
    FAsyncImportResult Result;
    bool bHasCompletedResult = false;

    {
        std::lock_guard<std::mutex> Lock(AsyncMutex);
        if (AsyncResult.bCompleted)
        {
            Result = AsyncResult;
            AsyncResult = {};
            bHasCompletedResult = true;
        }
    }

    if (!bHasCompletedResult)
    {
        return;
    }

    if (AsyncImportThread.joinable())
    {
        AsyncImportThread.join();
    }
    bAsyncImportRunning = false;

    if (!Result.bSucceeded)
    {
        FNotificationManager::Get().AddNotification(Result.ErrorMessage.empty() ? "Import failed: could not copy .uasset." : Result.ErrorMessage,
                                                    ENotificationType::Error,
                                                    5.0f);
        return;
    }

    FNotificationManager::Get().AddNotification("Import 90%: refreshing asset browser", ENotificationType::Info, 1.5f);
    FMeshAssetManager::ScanMeshAssets();
    FMeshAssetManager::ScanMeshSourceFiles();
    FMaterialManager::Get().ScanMaterialAssets();
    UTexture2D::ScanTextureAssets();

    if (EditorEngine)
    {
        EditorEngine->RefreshContentBrowser();
        if (!Result.ImportedAssetPath.empty())
        {
            EditorEngine->SelectContentBrowserPath(Result.ImportedAssetPath);
        }
    }

    const std::filesystem::path SourceAbsolute = ResolveSourcePathOnDisk(Result.SourcePath);
    const FString SourceFileName = FPaths::ToUtf8(SourceAbsolute.filename().wstring());
    const FString ImportedFileName = !Result.ImportedAssetPath.empty()
        ? FPaths::ToUtf8(std::filesystem::path(FPaths::ToWide(Result.ImportedAssetPath)).filename().wstring())
        : FString("asset");
    const FString Message = !Result.ImportedAssetPath.empty()
        ? "Imported " + SourceFileName + " as " + ImportedFileName
        : "Imported " + SourceFileName;

    FNotificationManager::Get().AddNotification(Message, ENotificationType::Success, 3.0f);
    UE_LOG_CATEGORY(AssetImport, Info, "[Import] %s | source=%s target=%s", Message.c_str(), ToUtf8GenericPath(SourceAbsolute).c_str(), Result.ImportedAssetPath.c_str());
}

void FAssetImportManager::StartAsyncImportWorker(const FString& SourcePath)
{
    const std::filesystem::path SourceAbsolute = ResolveSourcePathOnDisk(SourcePath);
    const FString SourceFileName = FPaths::ToUtf8(SourceAbsolute.filename().wstring());
    FNotificationManager::Get().AddNotification("Import 0%: preparing " + SourceFileName, ENotificationType::Info, 1.5f);

    FString ImportedAssetPath;
    FString ErrorMessage;
    const bool bSucceeded = ImportExistingUAssetWithProgress(SourcePath, &ImportedAssetPath, &ErrorMessage);

    std::lock_guard<std::mutex> Lock(AsyncMutex);
    AsyncResult.bCompleted = true;
    AsyncResult.bSucceeded = bSucceeded;
    AsyncResult.SourcePath = SourcePath;
    AsyncResult.ImportedAssetPath = ImportedAssetPath;
    AsyncResult.ErrorMessage = ErrorMessage;
}

bool FAssetImportManager::ImportExistingUAssetWithProgress(const FString& SourcePath, FString* OutImportedAssetPath, FString* OutErrorMessage)
{
    const std::filesystem::path SourceAbsolute = ResolveSourcePathOnDisk(SourcePath);
    FAssetFileHeader Header;
    if (!FAssetFileSerializer::ReadAssetHeader(SourceAbsolute, Header))
    {
        if (OutErrorMessage)
        {
            *OutErrorMessage = "Import failed: invalid .uasset file.";
        }
        return false;
    }

    const wchar_t* Category = L"Imported";
    switch (Header.ClassId)
    {
    case EAssetClassId::StaticMesh:
    case EAssetClassId::SkeletalMesh:
        Category = L"Meshes";
        break;
    case EAssetClassId::Material:
        Category = L"Materials";
        break;
    case EAssetClassId::Texture:
        Category = L"Textures";
        break;
    case EAssetClassId::PoseAsset:
        Category = L"Poses";
        break;
    default:
        Category = L"Imported";
        break;
    }

    std::filesystem::path DestinationDirectory = std::filesystem::path(FPaths::ContentDir()) / Category;
    std::filesystem::create_directories(DestinationDirectory);
    std::filesystem::path DestinationPath = DestinationDirectory / SourceAbsolute.filename();
    EnsureUniquePath(DestinationPath);

    UE_LOG_CATEGORY(AssetImport, Info, "[Import] Existing %s copy begin: source=%s target=%s",
                    GetAssetClassIdName(Header.ClassId),
                    ToUtf8GenericPath(SourceAbsolute).c_str(),
                    ToUtf8GenericPath(DestinationPath).c_str());

    int32 NextMilestone = 25;
    auto LastNotificationTime = std::chrono::steady_clock::now();
    const bool bCopied = CopyFileWithProgress(
        SourceAbsolute,
        DestinationPath,
        [SourceAbsolute, &NextMilestone, &LastNotificationTime](uint64 CopiedBytes, uint64 TotalBytes)
        {
            if (TotalBytes == 0)
            {
                return;
            }

            const int32 Percent = static_cast<int32>((CopiedBytes * 100ull) / TotalBytes);
            const auto Now = std::chrono::steady_clock::now();
            if (Percent < NextMilestone && (Now - LastNotificationTime) < std::chrono::milliseconds(300))
            {
                return;
            }

            while (Percent >= NextMilestone)
            {
                NextMilestone += 25;
            }

            LastNotificationTime = Now;
            const FString SourceFileName = FPaths::ToUtf8(SourceAbsolute.filename().wstring());
            FNotificationManager::Get().AddNotification("Import " + std::to_string((std::min)(Percent, 85)) + "%: copying " + SourceFileName,
                                                        ENotificationType::Info,
                                                        1.2f);
        },
        OutErrorMessage);
    if (!bCopied)
    {
        return false;
    }

    if (OutImportedAssetPath)
    {
        *OutImportedAssetPath = MakeProjectRelativePath(DestinationPath);
    }

    UE_LOG_CATEGORY(AssetImport, Info, "[Import] Existing %s copied into Content: %s",
                    GetAssetClassIdName(Header.ClassId),
                    MakeProjectRelativePath(DestinationPath).c_str());
    return true;
}
