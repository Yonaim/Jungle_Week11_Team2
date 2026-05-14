#pragma once

#include "Render/Proxy/PrimitiveSceneProxy.h"

class USkinnedMeshComponent;

// FSkeletalMeshProxy:
// SkinnedMesh 컴포넌트의 렌더 전용 미러.
// - CPU Skinning 결과 정점을 동적 VB로 업로드
// - 애셋 인덱스 버퍼를 정적 IB로 유지
// - SectionDraws를 통해 DrawCommandBuilder에 제출 단위를 제공
class FSkeletalMeshProxy : public FPrimitiveSceneProxy
{
public:
	static constexpr uint32 MAX_LOD = 4;

	FSkeletalMeshProxy(USkinnedMeshComponent* InComponent);

	bool HasValidGeometry() const override;
	void FillDrawCommandBuffer(FDrawCommandBuffer& OutBuffer) const override;

	void UpdateMesh() override;
	void UpdateMaterial() override;
	void UpdateLOD(uint32 LODLevel) override;
	void UpdatePerViewport(const FFrameContext& Frame) override;

private:
	// 머티리얼/섹션 변경 시 Draw 단위를 재구성한다.
	void RebuildSectionDraws();
	USkinnedMeshComponent* GetSkinnedMeshComponent() const;

	struct FSkeletalLODDrawData
	{
		FDynamicVertexBuffer DynamicVB;
		FIndexBuffer StaticIB;
		TArray<FMeshSectionDraw> SectionDraws;
		uint32 VertexCount = 0;
		uint32 IndexCount = 0;
	};

	FSkeletalLODDrawData LODData[MAX_LOD];
	// 현재 SkeletalMesh는 LOD0만 사용한다.
	// 다중 LOD 확장 시 UpdateMesh/RebuildSectionDraws에서 함께 확장한다.
	uint32 LODCount = 1;
};
