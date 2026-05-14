#pragma once

#include "Core/CoreTypes.h"
#include <atomic>
#include <deque>
#include <filesystem>
#include <mutex>
#include <thread>

class UObject;
class UEditorEngine;

class FAssetImportManager
{
public:
    void Init(UEditorEngine* InEditorEngine);
    void Shutdown();
    void Tick();

    // Unreal식 단일 Import 진입점.
    // 외부 원본 파일(.fbx/.obj/.mtl/texture/.uasset)을 받아 Asset/Content 아래 정식 .uasset으로 만든다.
    bool ImportAssetWithDialog();
    bool ImportAssetFromPath(const FString& SourcePath, FString* OutImportedAssetPath = nullptr);
    bool QueueImportAssetFromPath(const FString& SourcePath);

    // 기존 호출부 호환용 wrapper. 내부적으로는 ImportAssetWithDialog()만 사용한다.
    bool ImportMaterialWithDialog();
    bool ImportTextureWithDialog();

private:
    bool ImportMeshSource(const FString& SourcePath, FString* OutImportedAssetPath);
    bool ImportMtlSource(const FString& SourcePath, FString* OutImportedAssetPath);
    bool ImportTextureSource(const FString& SourcePath, FString* OutImportedAssetPath);
    bool ImportExistingUAsset(const FString& SourcePath, FString* OutImportedAssetPath);

    bool SaveImportedObject(const std::filesystem::path& DestinationPath, UObject* Object, FString* OutImportedAssetPath);
    std::filesystem::path MakeDestinationAssetPath(const FString& SourcePath, const wchar_t* CategoryDir, const wchar_t* Prefix) const;
    FString MakeProjectRelativePath(const std::filesystem::path& Path) const;
    bool ShouldImportAsync(const FString& SourcePath) const;
    void StartNextAsyncImportIfIdle();
    void FinalizeAsyncImportIfReady();
    void StartAsyncImportWorker(const FString& SourcePath);
    bool ImportExistingUAssetWithProgress(const FString& SourcePath, FString* OutImportedAssetPath, FString* OutErrorMessage);

private:
    UEditorEngine* EditorEngine = nullptr;

    struct FPendingAsyncImport
    {
        FString SourcePath;
    };

    struct FAsyncImportResult
    {
        bool bCompleted = false;
        bool bSucceeded = false;
        FString SourcePath;
        FString ImportedAssetPath;
        FString ErrorMessage;
    };

    std::mutex AsyncMutex;
    std::deque<FPendingAsyncImport> PendingAsyncImports;
    std::thread AsyncImportThread;
    std::atomic<bool> bAsyncImportRunning = false;
    FAsyncImportResult AsyncResult;
};
