#pragma once

#include "Render/Proxy/PrimitiveSceneProxy.h"

class UStaticMeshComponent;

// ============================================================
// FStaticMeshSceneProxy:
// UStaticMeshComponent의 렌더 전용 미러.
// - 컴포넌트/애셋에서 섹션별 Draw 단위를 추출해 SectionDraws를 구성
// - DrawCommandBuilder는 이 프록시만 읽어 렌더 커맨드를 생성
// - LOD 전환 시 활성 MeshBuffer/SectionDraws를 스왑
class FStaticMeshSceneProxy : public FPrimitiveSceneProxy
{
public:
	static constexpr uint32 MAX_LOD = 4;

	FStaticMeshSceneProxy(UStaticMeshComponent* InComponent);

	void UpdateMaterial() override;
	void UpdateMesh() override;
	void UpdateLOD(uint32 LODLevel) override;

private:
	UStaticMeshComponent* GetStaticMeshComponent() const;

	// 모든 LOD의 SectionDraws 재구축
	void RebuildSectionDraws();

	struct FLODDrawData
	{
		FMeshBuffer* MeshBuffer = nullptr;
		TArray<FMeshSectionDraw> SectionDraws;
	};

	FLODDrawData LODData[MAX_LOD];
	uint32 LODCount = 1;
};
