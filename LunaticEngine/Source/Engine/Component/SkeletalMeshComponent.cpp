#include "PCH/LunaticPCH.h"
#include "SkeletalMeshComponent.h"
#include "Serialization/Archive.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshCommon.h"
#include "Core/Log.h"
#include "Math/Quat.h"

// 시연용 — 랜덤 본 진동
#include <random>
#include <cmath>

IMPLEMENT_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

namespace
{
	bool IsFiniteVectorValue(const FVector& Value)
	{
		return std::isfinite(Value.X) && std::isfinite(Value.Y) && std::isfinite(Value.Z);
	}

	bool IsFiniteMatrixValue(const FMatrix& Value)
	{
		for (int32 Row = 0; Row < 4; ++Row)
		{
			for (int32 Column = 0; Column < 4; ++Column)
			{
				if (!std::isfinite(Value.M[Row][Column]))
				{
					return false;
				}
			}
		}
		return true;
	}
}

bool USkeletalMeshComponent::SetBoneLocalTransform(int32 BoneIndex, const FTransform& LocalTransform)
{
	return CurrentPose.SetLocalTransform(BoneIndex, LocalTransform);
}


bool USkeletalMeshComponent::ApplyLocalPoseTransforms(const TArray<FTransform>& LocalTransforms)
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset())
	{
		return false;
	}

	const int32 BoneCount = static_cast<int32>(SkeletalMesh->GetSkeletalMeshAsset()->Bones.size());
	if (BoneCount <= 0 || static_cast<int32>(LocalTransforms.size()) != BoneCount)
	{
		return false;
	}

	CurrentPose.LocalTransforms = LocalTransforms;
	++CurrentPose.PoseVersion;
	CurrentPose.bComponentDirty = true;
	RefreshSkinningNow();
	return true;
}

void USkeletalMeshComponent::ResetToBindPose()
{
	InitBoneTransform();
	RefreshSkinningNow();
}

void USkeletalMeshComponent::SetBoneLocalTransformByName(const FString& BoneName, const FTransform& LocalTransform)
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset())
		return;

	const TArray<FBoneInfo>& Bones = SkeletalMesh->GetSkeletalMeshAsset()->Bones;
	for (int32 i = 0; i < (int32)Bones.size(); ++i)
	{
		if (Bones[i].Name == BoneName)
		{
			SetBoneLocalTransform(i, LocalTransform);
			return;
		}
	}
}

void USkeletalMeshComponent::SetPreviewPoseForEditor(const FSkeletonPose& InPose)
{
	CurrentPose = InPose;
	RefreshSkinningNow();
}

// 메시 할당 시 즉시 초기화 — 에디터에서 BeginPlay 없이도 렌더링되도록
void USkeletalMeshComponent::SetSkeletalMesh(USkeletalMesh* Mesh)
{
	UE_LOG_CATEGORY(Component, Info, "[SkeletalMeshComponent] SetSkeletalMesh begin: mesh=%s",
		Mesh ? Mesh->GetFName().ToString().c_str() : "None");
	USkinnedMeshComponent::SetSkeletalMesh(Mesh);
	InitializeSkeleton();
	UE_LOG_CATEGORY(Component, Info, "[SkeletalMeshComponent] SetSkeletalMesh complete");
}

void USkeletalMeshComponent::InitializeSkeleton()
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset())
		return;

	UE_LOG_CATEGORY(Component, Info, "[SkeletalMeshComponent] InitializeSkeleton: bones=%d vertices=%d indices=%d",
		SkeletalMesh->GetBoneCount(),
		SkeletalMesh->GetVertexCount(),
		SkeletalMesh->GetIndexCount());

	const FSkeletalMesh* MeshAsset = SkeletalMesh->GetSkeletalMeshAsset();
	InitBoneTransform();

	// SkinBuffer는 매 프레임 갱신되는 런타임 결과 버퍼이며,
	// 원본 애셋 정점은 참조 포즈 보존용으로 유지한다.
	SkinBuffer = MeshAsset->Vertices;

	RebuildComponentSpace();
	PerformCPUSkinning(CurrentPose);
	FinalizeRenderState();
}

void USkeletalMeshComponent::BeginPlay()
{
	InitializeSkeleton();
	InitializeRandomBoneOscillations();
}

void USkeletalMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset())
		return;

	EvaluatePose(DeltaTime);
	UpdatePoseLocal(DeltaTime);
	RebuildComponentSpace();
	PerformCPUSkinning(CurrentPose);
	FinalizeRenderState();
}

void USkeletalMeshComponent::RefreshSkinningForEditor(float DeltaTime)
{
    if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset())
        return;

    EvaluatePose(DeltaTime);
    UpdatePoseLocal(DeltaTime);
    RebuildComponentSpace();
    PerformCPUSkinning(CurrentPose);
    FinalizeRenderState();
}

void USkeletalMeshComponent::RefreshSkinningNow()
{
	UE_LOG_CATEGORY(Component, Info, "[SkeletalMeshComponent] RefreshSkinningNow");
	RebuildComponentSpace();
	PerformCPUSkinning(CurrentPose);
	FinalizeRenderState();
}

void USkeletalMeshComponent::EvaluatePose(float DeltaTime)
{
	// 시연용
	if (!bEnableBoneRotationTest || BoneOscillations.empty())
		return;

	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset())
		return;

	OscillationTime += DeltaTime;

	const TArray<FBoneInfo>& Bones = SkeletalMesh->GetSkeletalMeshAsset()->Bones;

	// 매 프레임 바인드 포즈에서 시작해 사인파 회전을 덮어씀
	// (누적 오류 없이 시간축에서 직접 계산)
	InitBoneTransform();

	constexpr float TwoPi = 6.28318530718f;

	for (const FBoneOscillationState& Osc : BoneOscillations)
	{
		if (Osc.BoneIndex < 0 || Osc.BoneIndex >= (int32)Bones.size())
			continue;

		const float Pitch = Osc.AmpPitch * sinf(TwoPi * Osc.FreqPitch * OscillationTime + Osc.PhasePitch);
		const float Yaw   = Osc.AmpYaw   * sinf(TwoPi * Osc.FreqYaw   * OscillationTime + Osc.PhaseYaw);
		const float Roll  = Osc.AmpRoll  * sinf(TwoPi * Osc.FreqRoll  * OscillationTime + Osc.PhaseRoll);

		const FTransform& BindLocal = Bones[Osc.BoneIndex].LocalBindTransform;
		FQuat DeltaQuat = FRotator(Pitch, Yaw, Roll).ToQuaternion();
		FQuat NewRotation = (BindLocal.Rotation * DeltaQuat).GetNormalized();

		FTransform NewTransform = BindLocal;
		NewTransform.Rotation = NewRotation;
		SetBoneLocalTransform(Osc.BoneIndex, NewTransform);
	}
}

void USkeletalMeshComponent::UpdatePoseLocal(float DeltaTime)
{
	(void)DeltaTime;
	// EvaluatePose에서 LocalTransforms를 직접 갱신하는 현재 구조를 유지한다.
}

void USkeletalMeshComponent::RebuildComponentSpace()
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset())
		return;

	const TArray<FBoneInfo>& Bones = SkeletalMesh->GetSkeletalMeshAsset()->Bones;
	CurrentPose.RebuildComponentSpace(Bones);
}

void USkeletalMeshComponent::PerformCPUSkinning(const FSkeletonPose& Pose)
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset())
		return;

	const FSkeletalMesh* MeshAsset = SkeletalMesh->GetSkeletalMeshAsset();
	const TArray<FNormalVertex>& BindVerts = MeshAsset->Vertices;
	const TArray<FSkinWeight>& SkinWeights = MeshAsset->SkinWeights;
	const TArray<FBoneInfo>& Bones = MeshAsset->Bones;
	const TArray<FMatrix>& ComponentSpaceTransforms = Pose.ComponentTransforms;
	const int32 VertexCount = static_cast<int32>(BindVerts.size());
	const int32 SkinWeightCount = static_cast<int32>(SkinWeights.size());
	const int32 BoneCount = static_cast<int32>(Bones.size());
	const int32 PoseTransformCount = static_cast<int32>(ComponentSpaceTransforms.size());

	if (static_cast<int32>(SkinBuffer.size()) != VertexCount)
	{
		SkinBuffer.resize(VertexCount);
	}

	if (VertexCount <= 0)
	{
		return;
	}

	if (SkinWeightCount < VertexCount)
	{
		UE_LOG_CATEGORY(Component, Warning,
			"SkeletalMesh CPU skinning fallback: mesh=%s vertices=%d skinWeights=%d",
			MeshAsset->PathFileName.c_str(),
			VertexCount,
			SkinWeightCount);

		for (int32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
		{
			SkinBuffer[VertexIndex] = BindVerts[VertexIndex];
		}
		return;
	}

	if (PoseTransformCount < BoneCount)
	{
		UE_LOG_CATEGORY(Component, Warning,
			"SkeletalMesh CPU skinning fallback: mesh=%s bones=%d poseTransforms=%d",
			MeshAsset->PathFileName.c_str(),
			BoneCount,
			PoseTransformCount);

		for (int32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
		{
			SkinBuffer[VertexIndex] = BindVerts[VertexIndex];
		}
		return;
	}

	// CPU Skinning 흐름:
	// Reference Pose 정점
	// -> 영향 본 반복
	// -> (InverseBindPose * CurrentComponentSpace) 스키닝 행렬 적용
	// -> 가중치 누적
	// -> SkinBuffer에 현재 포즈 정점/노멀 기록
	for (int32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
	{
		const FNormalVertex& BindVertex = BindVerts[VertexIndex];
		const FSkinWeight& Weight = SkinWeights[VertexIndex];

		FVector SkinnedPos(0.0f, 0.0f, 0.0f);
		FVector SkinnedNormal(0.0f, 0.0f, 0.0f);
		float TotalWeight = 0.0f;

		for (int32 Inf = 0; Inf < MaxBoneInfluences; ++Inf)
		{
			const int32 BoneIdx = Weight.BoneIndices[Inf];
			const float W = Weight.BoneWeights[Inf];

			if (BoneIdx == InvalidBoneIndex || W <= 0.0f)
				continue;

			if (BoneIdx < 0 || BoneIdx >= BoneCount || BoneIdx >= PoseTransformCount)
			{
				UE_LOG_CATEGORY(Component, Warning,
					"SkeletalMesh CPU skinning skipped invalid bone influence: mesh=%s vertex=%d influence=%d boneIndex=%d bones=%d poseTransforms=%d",
					MeshAsset->PathFileName.c_str(),
					VertexIndex,
					Inf,
					BoneIdx,
					BoneCount,
					PoseTransformCount);
				continue;
			}

			// 현재 코드의 행렬 곱 순서를 그대로 사용한다.
			// 수학 규약(row/column major)에 따라 의미가 달라질 수 있어 추후 검증 포인트다.
			const FMatrix SkinningMatrix = Bones[BoneIdx].InverseBindPose * ComponentSpaceTransforms[BoneIdx];
			if (!IsFiniteMatrixValue(SkinningMatrix))
			{
				continue;
			}

			SkinnedPos += SkinningMatrix.TransformPositionWithW(BindVertex.pos) * W;

			// 노멀은 position/vector와 다르게 inverse-transpose로 변환해야 한다.
			// 현재 포즈에 non-uniform scale/negative scale이 섞여도 WorldNormal View가 무너지지 않도록
			// 스키닝 행렬의 선형부 기준 normal matrix를 사용한다.
			const FMatrix NormalMatrix = std::fabs(SkinningMatrix.GetBasisDeterminant3x3()) > 1.0e-8f
				? SkinningMatrix.GetInverse().GetTransposed()
				: SkinningMatrix;
			SkinnedNormal += NormalMatrix.TransformVector(BindVertex.normal) * W;
			TotalWeight += W;
		}

		SkinBuffer[VertexIndex] = BindVertex;
		if (TotalWeight > 0.0f && IsFiniteVectorValue(SkinnedPos) && IsFiniteVectorValue(SkinnedNormal))
		{
			SkinBuffer[VertexIndex].pos = SkinnedPos;
			SkinBuffer[VertexIndex].normal = SkinnedNormal.Normalized();
		}
	}
}

void USkeletalMeshComponent::FinalizeRenderState()
{
	// 본/스키닝 결과 변경을 렌더러와 바운드 갱신 경로에 알린다.
	MarkWorldBoundsDirty();
}



// 시연용
void USkeletalMeshComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	USkinnedMeshComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Enable Bone Random Motion", EPropertyType::Bool, &bEnableBoneRotationTest });
}

void USkeletalMeshComponent::PostEditProperty(const char* PropertyName)
{
	USkinnedMeshComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Enable Bone Random Motion") == 0)
	{
		// 토글 off 시 바인드 포즈로 즉시 복원
		if (!bEnableBoneRotationTest)
		{
			BoneOscillations.clear();
			OscillationTime = 0.f;
			InitBoneTransform();
			RebuildComponentSpace();
			PerformCPUSkinning(CurrentPose);
			FinalizeRenderState();
		}
	}
}

void USkeletalMeshComponent::Serialize(FArchive& Ar)
{
	USkinnedMeshComponent::Serialize(Ar);
	Ar << bEnableBoneRotationTest;
}

void USkeletalMeshComponent::InitializeRandomBoneOscillations()
{
	BoneOscillations.clear();
	OscillationTime = 0.f;

	if (!bEnableBoneRotationTest)
		return;

	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset())
		return;

	const TArray<FBoneInfo>& Bones = SkeletalMesh->GetSkeletalMeshAsset()->Bones;
	const int32 BoneCount = (int32)Bones.size();
	if (BoneCount <= 1)
		return;

	// 재현 가능한 고정 시드 — 매번 같은 패턴으로 자연스러운 느낌 유지
	std::mt19937 Rng(0xDEADBEEF);
	std::uniform_real_distribution<float> FreqDist(0.08f, 0.35f); // Hz — 느린 유기적 움직임
	std::uniform_real_distribution<float> PhaseDist(0.f, 6.2832f);
	std::uniform_real_distribution<float> AmpPitchDist(2.f, 12.f); // degrees
	std::uniform_real_distribution<float> AmpYawDist(3.f, 18.f);
	std::uniform_real_distribution<float> AmpRollDist(1.f, 7.f);
	std::uniform_int_distribution<int>    SkipDist(0, 2); // 약 1/3 확률로 건너뜀

	for (int32 i = 0; i < BoneCount; ++i)
	{
		// 루트 본은 전체 메시를 흔들어버리므로 제외
		if (Bones[i].ParentIndex == InvalidBoneIndex)
			continue;

		// 무작위 선별 (~67% 포함)
		if (SkipDist(Rng) == 0)
			continue;

		FBoneOscillationState Osc;
		Osc.BoneIndex = i;
		// 각 축을 독립적인 주파수·위상으로 설정해 서로 다른 리듬 생성
		Osc.FreqPitch = FreqDist(Rng);
		Osc.FreqYaw = FreqDist(Rng);
		Osc.FreqRoll = FreqDist(Rng);
		Osc.AmpPitch = AmpPitchDist(Rng);
		Osc.AmpYaw = AmpYawDist(Rng);
		Osc.AmpRoll = AmpRollDist(Rng);
		Osc.PhasePitch = PhaseDist(Rng);
		Osc.PhaseYaw = PhaseDist(Rng);
		Osc.PhaseRoll = PhaseDist(Rng);

		BoneOscillations.push_back(Osc);
	}
}
