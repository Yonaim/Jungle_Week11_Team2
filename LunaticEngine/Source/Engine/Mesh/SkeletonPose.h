#pragma once

#include "Core/CoreTypes.h"
#include "Math/Transform.h"
#include "Math/Matrix.h"
#include "Mesh/SkeletalMeshCommon.h"

// FSkeletonPose:
// SkeletalMesh 컴포넌트 인스턴스가 소유하는 "현재 포즈" 런타임 데이터.
// 애셋(FSkeletalMesh)의 Bind/Reference 데이터와 분리되어,
// 애니메이션 평가/뷰어 미리보기/스키닝 입력으로 재사용된다.
struct FSkeletonPose
{
	TArray<FTransform> LocalTransforms;
	TArray<FMatrix> ComponentTransforms;

	// 포즈 갱신 추적용 버전 카운터.
	uint32 PoseVersion = 0;

	// Local이 바뀌면 Component 재계산이 필요하다.
	bool bComponentDirty = false;

	void Reset()
	{
		LocalTransforms.clear();
		ComponentTransforms.clear();
		PoseVersion = 0;
		bComponentDirty = false;
	}

	void InitializeFromBindPose(const FSkeletalMesh& MeshAsset)
	{
		const int32 BoneCount = static_cast<int32>(MeshAsset.Bones.size());
		LocalTransforms.resize(BoneCount);
		ComponentTransforms.resize(BoneCount);

		for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
		{
			LocalTransforms[BoneIndex] = MeshAsset.Bones[BoneIndex].LocalBindTransform;
			ComponentTransforms[BoneIndex] = FMatrix::Identity;
		}

		++PoseVersion;
		bComponentDirty = true;
	}

	bool SetLocalTransform(int32 BoneIndex, const FTransform& LocalTransform)
	{
		if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(LocalTransforms.size()))
		{
			return false;
		}

		LocalTransforms[BoneIndex] = LocalTransform;
		++PoseVersion;
		bComponentDirty = true;
		return true;
	}

	void RebuildComponentSpace(const TArray<FBoneInfo>& Bones)
	{
		const int32 BoneCount = static_cast<int32>(Bones.size());
		if (static_cast<int32>(LocalTransforms.size()) != BoneCount)
		{
			return;
		}

		if (static_cast<int32>(ComponentTransforms.size()) != BoneCount)
		{
			ComponentTransforms.resize(BoneCount);
		}

		for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
		{
			const FMatrix LocalMatrix = LocalTransforms[BoneIndex].ToMatrix();
			const int32 ParentIndex = Bones[BoneIndex].ParentIndex;

			if (ParentIndex == InvalidBoneIndex)
			{
				ComponentTransforms[BoneIndex] = LocalMatrix;
			}
			else
			{
				ComponentTransforms[BoneIndex] = LocalMatrix * ComponentTransforms[ParentIndex];
			}
		}

		bComponentDirty = false;
	}
};
