#pragma once

#include "Core/CoreTypes.h"
#include "Mesh/StaticMeshCommon.h"
#include "Math/Transform.h"
#include "Math/Vector.h"

#include "Engine/Object/Object.h"
#include "Engine/Object/FName.h"
#include "Render/Resource/Buffer.h"
#include "Serialization/Archive.h"

#include "Materials/Material.h"
#include "Materials/MaterialManager.h"

#include <memory>
#include <algorithm>

constexpr int32 MaxBoneInfluences = 4;
constexpr int32 InvalidBoneIndex = -1;

inline FArchive& SerializeSkeletalTransform(FArchive& Ar, FTransform& Transform)
{
	Ar << Transform.Location;
	Ar << Transform.Rotation.X;
	Ar << Transform.Rotation.Y;
	Ar << Transform.Rotation.Z;
	Ar << Transform.Rotation.W;
	Ar << Transform.Scale;
	return Ar;
}

inline FArchive& SerializeSkeletalMatrix(FArchive& Ar, FMatrix& Matrix)
{
	for (int32 Row = 0; Row < 4; ++Row)
	{
		for (int32 Column = 0; Column < 4; ++Column)
		{
			Ar << Matrix.M[Row][Column];
		}
	}
	return Ar;
}

// 정점당 본 영향 정보.
// 한 정점은 최대 MaxBoneInfluences개의 본 인덱스/가중치를 가진다.
struct FSkinWeight
{
	int32 BoneIndices[MaxBoneInfluences] =
	{
		InvalidBoneIndex,
		InvalidBoneIndex,
		InvalidBoneIndex,
		InvalidBoneIndex
	};

	float BoneWeights[MaxBoneInfluences] =
	{
		0.0f,
		0.0f,
		0.0f,
		0.0f
	};
};

// 본 계층 및 바인드 포즈 정보.
// - LocalBindTransform: 부모 기준 로컬 바인드 포즈
// - InverseBindPose: 메시 공간 바인드 글로벌 행렬의 역행렬
//   (현재 포즈 글로벌과 결합해 스키닝 행렬을 만들 때 사용)
struct FBoneInfo
{
	std::string Name;

	// 루트 본은 ParentIndex가 InvalidBoneIndex.
	int32 ParentIndex = InvalidBoneIndex;

	FTransform LocalBindTransform;
	FMatrix InverseBindPose;

	friend FArchive& operator<<(FArchive& Ar, FBoneInfo& Bone)
	{
		Ar << Bone.Name;
		Ar << Bone.ParentIndex;
		SerializeSkeletalTransform(Ar, Bone.LocalBindTransform);
		SerializeSkeletalMatrix(Ar, Bone.InverseBindPose);
		return Ar;
	}
};

// SkeletalMesh 섹션 정보(대개 머티리얼 기준 서브메시).
struct FSkeletalMeshSection
{
	int32 MaterialIndex = 0;

	uint32 IndexStart = 0;
	uint32 IndexCount = 0;

	uint32 VertexStart = 0;
	uint32 VertexCount = 0;
};

// SkeletalMesh Cooked Data 본체.
// - Vertices: Reference Pose 기준 원본 정점
// - SkinWeights/Bones: 스키닝 입력 데이터
// - Sections: 렌더링 섹션 정보
struct FSkeletalMesh
{
	FString PathFileName;
	TArray<FNormalVertex> Vertices;
	TArray<uint32> Indices;
	TArray<FSkinWeight> SkinWeights;
	TArray<FBoneInfo> Bones;

	TArray<TArray<int32>> BoneChildren;
	TArray<int32> RootBoneIndices;

	TArray<FSkeletalMeshSection> Sections;

	void Serialize(FArchive& Ar)
	{
		Ar << PathFileName;
		Ar << Vertices;
		Ar << Indices;
		Ar << SkinWeights;
		Ar << Bones;
		Ar << Sections;
	}

	void BuildBoneHierarchyCache()
	{
		BoneChildren.clear();
		RootBoneIndices.clear();

		const int32 BoneCount = static_cast<int32>(Bones.size());
		BoneChildren.resize(BoneCount);

		for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
		{
			const int32 ParentIndex = Bones[BoneIndex].ParentIndex;

			if (ParentIndex == InvalidBoneIndex ||
				ParentIndex < 0 ||
				ParentIndex >= BoneCount)
			{
				RootBoneIndices.push_back(BoneIndex);
			}
			else
			{
				BoneChildren[ParentIndex].push_back(BoneIndex);
			}
		}
	}
};
