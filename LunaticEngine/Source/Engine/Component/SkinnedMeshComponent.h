#pragma once
#include "Component/MeshComponent.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletonPose.h"

class FPrimitiveSceneProxy;

// USkinnedMeshComponent:
// SkeletalMesh 계열 컴포넌트의 공통 기반 클래스.
// - 애셋(USkeletalMesh) 참조 보관
// - 본 포즈 런타임 캐시(FSkeletonPose) 관리
// - 스키닝 결과 기반 월드 AABB 갱신 공통 처리
class USkinnedMeshComponent : public UMeshComponent
{
public:
	DECLARE_CLASS(USkinnedMeshComponent, UMeshComponent)

	USkinnedMeshComponent() = default;
	~USkinnedMeshComponent() override = default;

	virtual void SetSkeletalMesh(USkeletalMesh* Mesh);
	USkeletalMesh* GetSkeletalMesh() const;

	void SetMaterial(int32 ElementIndex, UMaterial* InMaterial);
	UMaterial* GetMaterial(int32 ElementIndex) const;
	const TArray<UMaterial*>& GetOverrideMaterials() const { return OverrideMaterials; }
	void EnsureMaterialSlotsForEditing();

	// 하위 코드 호환을 위해 기존 Getter 시그니처는 유지한다.
	const TArray<FTransform>& GetBoneSpaceTransforms() const { return CurrentPose.LocalTransforms; }
	const TArray<FMatrix>& GetComponentSpaceTransforms() const { return CurrentPose.ComponentTransforms; }
	const FSkeletonPose& GetCurrentPose() const { return CurrentPose; }

	FTransform GetBoneWorldTransform(int32 BoneIndex) const;
	FTransform GetBoneWorldTransformByName(const FString& BoneName) const;

	const FTransform* GetBoneLocalTransform(int32 BoneIndex) const;

	int32 GetBoneCount() const;

	// CPU Skinning 결과 정점 버퍼 접근점.
	// 기본 클래스는 데이터를 소유하지 않으므로 nullptr를 반환하고,
	// 실제 구현(USkeletalMeshComponent)에서 런타임 버퍼를 노출한다.
	virtual TArray<FNormalVertex>* GetCPUSkinnedVertices() { return nullptr; }

	FPrimitiveSceneProxy* CreateSceneProxy() override;
	void UpdateWorldAABB() const override;

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;
	void Serialize(FArchive& Ar) override;
	void PostDuplicate() override;

protected:
	// 애셋 소유는 USkeletalMesh가 담당하고, 컴포넌트는 참조만 유지한다.
	USkeletalMesh* SkeletalMesh = nullptr;
	FString SkeletalMeshPath = "None";

	// 인스턴스별 머티리얼 오버라이드(애셋 기본 슬롯을 덮어씀).
	TArray<UMaterial*> OverrideMaterials;
	// 직렬화/에디터 바인딩용 머티리얼 슬롯 상태.
	TArray<FMaterialSlot> MaterialSlots;

	// 컴포넌트 인스턴스 런타임 포즈 상태.
	FSkeletonPose CurrentPose;

	void InitBoneTransform();
};
