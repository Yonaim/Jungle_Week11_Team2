#pragma once

#include "Core/CoreTypes.h"
#include "Object/ObjectIterator.h"
#include "Render/Types/RenderTypes.h"
#include "Engine/Asset/AssetFileSerializer.h"
#include <memory>
#include <string>

struct FStaticMesh;
struct FStaticMaterial;
struct FImportOptions;
class UStaticMesh;
class USkeletalMesh;

struct FMeshAssetListItem
{
    FString DisplayName;
    FString FullPath;
    EAssetClassId AssetClassId = EAssetClassId::Unknown;
};

// Runtime/Level Editor는 같은 asset path를 공유 캐시로 써도 되지만,
// Asset Editor는 저장 전 변경사항이 Level Editor에 새어 나가면 안 되므로
// 같은 파일이라도 별도 working copy instance로 로드해야 한다.
enum class EMeshAssetLoadPurpose
{
    RuntimeShared,
    FreshInstance,
};

// Mesh 에셋의 source import(.obj/.fbx)와 cooked asset(.uasset) 로드를 담당한다.
// Obj 전용 관리자가 아니므로, OBJ 파싱 자체는 FObjImporter에만 남긴다.
class FMeshAssetManager
{
    // path → UObject mesh cache. 소유권은 UObjectManager가 가진다.
    static TMap<FString, UStaticMesh*> StaticMeshCache;
    static TMap<FString, USkeletalMesh*> SkeletalMeshCache;
    static TArray<FMeshAssetListItem> AvailableMeshFiles;
    static TArray<FMeshAssetListItem> AvailableMeshSourceFiles;

public:
    static FString GetStaticMeshAssetPath(const FString& SourceOrAssetPath);
    static FString GetSkeletalMeshAssetPath(const FString& SourceOrAssetPath);

    static UStaticMesh* LoadStaticMesh(const FString& PathFileName, ID3D11Device* InDevice);
    static UStaticMesh* LoadStaticMesh(const FString& PathFileName, const FImportOptions& Options, ID3D11Device* InDevice);
    static USkeletalMesh* LoadSkeletalMesh(const FString& PathFileName, ID3D11Device* InDevice);

    static UStaticMesh* LoadStaticMeshAssetFile(const FString& AssetPath, ID3D11Device* InDevice,
                                                EMeshAssetLoadPurpose Purpose = EMeshAssetLoadPurpose::RuntimeShared);
    static USkeletalMesh* LoadSkeletalMeshAssetFile(const FString& AssetPath, ID3D11Device* InDevice,
                                                    EMeshAssetLoadPurpose Purpose = EMeshAssetLoadPurpose::RuntimeShared);

    // 저장된 .uasset이 바뀐 뒤 다음 RuntimeShared load가 새 파일을 읽도록 캐시 key만 제거한다.
    // 기존 컴포넌트가 들고 있는 UObject 포인터는 여기서 파괴하지 않는다.
    static void MarkAssetCacheStale(const FString& AssetPath);

    static bool IsFbxSkeletalMesh(const FString& FbxPath);

    static void ScanMeshAssets();
    static const TArray<FMeshAssetListItem>& GetAvailableMeshFiles();

    static void ScanMeshSourceFiles();
    static const TArray<FMeshAssetListItem>& GetAvailableMeshSourceFiles();

    // 캐시된 StaticMesh GPU 리소스 해제 (Shutdown 시 Device 해제 전 호출)
    static void ReleaseAllGPU();

};
