#pragma once

#include "Object/Object.h"
#include "Collision/MeshTriangleBVH.h"
#include "Mesh/StaticMeshCommon.h"
#include "Serialization/Archive.h"

#include <memory>

struct ID3D11Device;

// LOD 단계별 렌더 리소스 묶음.
// 각 LOD는 섹션 정보와 GPU 버퍼를 함께 유지한다.
struct FLODMeshData
{
	TArray<FStaticMeshSection> Sections;
	std::unique_ptr<FMeshBuffer> RenderBuffer;
};

// UStaticMesh:
// 변형되지 않는(Non-deforming) StaticMesh 애셋 래퍼.
// - 정점/인덱스/섹션/바운드 같은 기하 데이터는 FStaticMesh가 소유
// - 이 클래스는 애셋 수명, 리소스 초기화, 머티리얼 슬롯, BVH 유틸을 담당
class UStaticMesh : public UObject
{
public:
	DECLARE_CLASS(UStaticMesh, UObject)

	static constexpr uint32 MAX_LOD_COUNT = 4;

	UStaticMesh() = default;
	~UStaticMesh() override;

	void Serialize(FArchive& Ar);

	const FString& GetAssetPathFileName() const;
	void SetStaticMeshAsset(FStaticMesh* InMesh);
	FStaticMesh* GetStaticMeshAsset() const;
	void SetStaticMaterials(TArray<FStaticMaterial>&& InMaterials);
	const TArray<FStaticMaterial>& GetStaticMaterials() const;

	void InitResources(ID3D11Device* InDevice);

	// 메시 로컬 공간 삼각형 BVH를 사용한 피킹 경로.
	// 월드 BVH가 후보를 줄인 뒤, 메시 내부 정밀 판정에서 사용된다.
	void EnsureMeshTrianglePickingBVHBuilt() const;
	bool RaycastMeshTrianglesWithBVHLocal(const FVector& LocalOrigin, const FVector& LocalDirection, FRayHitResult& OutHitResult) const;
	
	// LOD 접근
	uint32 GetLODCount() const { return bHasLOD ? MAX_LOD_COUNT : 1; }
	FMeshBuffer* GetLODMeshBuffer(uint32 LODLevel) const;
	const TArray<FStaticMeshSection>& GetLODSections(uint32 LODLevel) const;

private:
	void RefreshSectionMaterialIndices();

	FStaticMesh* StaticMeshAsset = nullptr;
	TArray<FStaticMaterial> StaticMaterials; // 섹션 슬롯명과 실제 머티리얼 연결 정보
	mutable FMeshTriangleBVH MeshTrianglePickingBVH; // 메시 로컬 정밀 레이캐스트 가속 구조

	// LOD0은 원본 StaticMeshAsset, AdditionalLODs는 단순화 LOD 캐시 슬롯.
	FLODMeshData AdditionalLODs[3];
	bool bHasLOD = false;
};
