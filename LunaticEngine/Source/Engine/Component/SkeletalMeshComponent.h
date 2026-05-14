#pragma once
#include "SkinnedMeshComponent.h"

// 시연용 — 랜덤 본 사인파 진동
struct FBoneOscillationState
{
	int32  BoneIndex  = -1;
	float  FreqPitch  = 0.f;  // Hz
	float  FreqYaw    = 0.f;
	float  FreqRoll   = 0.f;
	float  AmpPitch   = 0.f;  // degrees
	float  AmpYaw     = 0.f;
	float  AmpRoll    = 0.f;
	float  PhasePitch = 0.f;  // radians
	float  PhaseYaw   = 0.f;
	float  PhaseRoll  = 0.f;
};

// USkeletalMeshComponent:
// 실제 SkeletalMesh 인스턴스를 월드에 배치하고 렌더링하는 구체 컴포넌트.
// 본 포즈를 평가하고 CPU Skinning 결과를 렌더러가 사용할 버퍼로 유지한다.
class USkeletalMeshComponent : public USkinnedMeshComponent
{
public:
	DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

	USkeletalMeshComponent() = default;
	~USkeletalMeshComponent() override = default;

	bool SetBoneLocalTransform(int32 BoneIndex, const FTransform& LocalTransform);
	bool ApplyLocalPoseTransforms(const TArray<FTransform>& LocalTransforms);
	void ResetToBindPose();
	void SetBoneLocalTransformByName(const FString& BoneName, const FTransform& LocalTransform);
	void SetPreviewPoseForEditor(const FSkeletonPose& InPose);

	TArray<FNormalVertex>* GetCPUSkinnedVertices() override { return &SkinBuffer; }

	void SetSkeletalMesh(USkeletalMesh* Mesh);
	void RefreshSkinningForEditor(float DeltaTime);
	void RefreshSkinningNow();

	void BeginPlay() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

	// 시연용
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;
	void Serialize(FArchive& Ar) override;

protected:
	void InitializeSkeleton();
	virtual void EvaluatePose(float DeltaTime);
	void UpdatePoseLocal(float DeltaTime);
	void RebuildComponentSpace();
	// Reference Pose 정점 + BoneIndex/Weight + 현재 본 행렬을 사용해 현재 포즈 정점을 계산한다.
	void PerformCPUSkinning(const FSkeletonPose& Pose);
	void FinalizeRenderState();

	// 컴포넌트 인스턴스가 소유하는 런타임 스키닝 결과.
	// 애셋 원본 정점(FSkeletalMesh::Vertices)은 보존하고 여기만 갱신한다.
	TArray<FNormalVertex> SkinBuffer;

	// 시연용 — 랜덤 본 진동 (에디터 토글, PIE에서 실행)
	bool bEnableBoneRotationTest = false;

	TArray<FBoneOscillationState> BoneOscillations;
	float OscillationTime = 0.f;

private:
	void InitializeRandomBoneOscillations();
};
