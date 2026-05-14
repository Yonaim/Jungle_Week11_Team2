#include "PCH/LunaticPCH.h"
#include "Mesh/SkeletalMesh.h"

IMPLEMENT_CLASS(USkeletalMesh, UObject)

USkeletalMesh::~USkeletalMesh()
{
	delete SkeletalMesh;
	SkeletalMesh = nullptr;
}

void USkeletalMesh::Serialize(FArchive& Ar)
{
	// 로딩 시점에 애셋 본체를 생성해 두고,
	// FSkeletalMesh가 정점/인덱스/스킨/본/섹션을 일괄 직렬화한다.
	if (Ar.IsLoading() && !SkeletalMesh)
	{
		SkeletalMesh = new FSkeletalMesh();
	}

	SkeletalMesh->Serialize(Ar);
	Ar << StaticMaterials;

	// BoneChildren / RootBoneIndices는 파일에 저장하지 않는 runtime cache다.
	// .uasset을 로드한 직후 재구성하지 않으면 Skeleton Tree가 비어 보인다.
	if (Ar.IsLoading() && SkeletalMesh)
	{
		SkeletalMesh->BuildBoneHierarchyCache();
	}
}

bool USkeletalMesh::IsValid() const
{
	return SkeletalMesh != nullptr;
}

int32 USkeletalMesh::GetBoneCount() const
{
	if (!SkeletalMesh)
	{
		return 0;
	}

	return static_cast<int32>(SkeletalMesh->Bones.size());
}

int32 USkeletalMesh::GetVertexCount(int32 LODIndex) const
{
	(void)LODIndex;
	return SkeletalMesh ? static_cast<int32>(SkeletalMesh->Vertices.size()) : 0;
}

int32 USkeletalMesh::GetIndexCount(int32 LODIndex) const
{
	(void)LODIndex;
	return SkeletalMesh ? static_cast<int32>(SkeletalMesh->Indices.size()) : 0;
}

void USkeletalMesh::SetSkeletalMeshAsset(FSkeletalMesh* InMesh)
{
	// 이전 애셋 메모리를 해제하고 새 Cooked Data로 교체한다.
	delete SkeletalMesh;
	SkeletalMesh = InMesh;
	if (SkeletalMesh)
	{
		SkeletalMesh->BuildBoneHierarchyCache();
	}
}

const TArray<FBoneInfo>& USkeletalMesh::GetBones() const
{
	static const TArray<FBoneInfo> EmptyBones;

	if (!SkeletalMesh)
	{
		return EmptyBones;
	}

	return SkeletalMesh->Bones;
}

const FBoneInfo* USkeletalMesh::GetBoneInfo(int32 BoneIndex) const
{
	if (!SkeletalMesh)
	{
		return nullptr;
	}

	const int32 BoneCount = static_cast<int32>(SkeletalMesh->Bones.size());

	if (BoneIndex < 0 || BoneIndex >= BoneCount)
	{
		return nullptr;
	}

	return &SkeletalMesh->Bones[BoneIndex];
}

int32 USkeletalMesh::GetParentBoneIndex(int32 BoneIndex) const
{
	const FBoneInfo* Bone = GetBoneInfo(BoneIndex);
	if (!Bone)
	{
		return InvalidBoneIndex;
	}

	return Bone->ParentIndex;
}

const char* USkeletalMesh::GetBoneName(int32 BoneIndex) const
{
	const FBoneInfo* Bone = GetBoneInfo(BoneIndex);
	if (!Bone)
	{
		return "";
	}

	return Bone->Name.c_str();
}

const TArray<TArray<int32>>& USkeletalMesh::GetBoneChildren() const
{
	static const TArray<TArray<int32>> EmptyBoneChildren;

	if (!SkeletalMesh)
	{
		return EmptyBoneChildren;
	}

	return SkeletalMesh->BoneChildren;
}

const TArray<int32>& USkeletalMesh::GetRootBoneIndices() const
{
	static const TArray<int32> EmptyRootBoneIndices;

	if (!SkeletalMesh)
	{
		return EmptyRootBoneIndices;
	}

	return SkeletalMesh->RootBoneIndices;
}

FSkeletalMesh* USkeletalMesh::GetSkeletalMeshAsset() const
{
	return SkeletalMesh;
}

void USkeletalMesh::SetStaticMaterials(TArray<FStaticMaterial>&& InMaterials)
{
	StaticMaterials = std::move(InMaterials);
}

const TArray<FStaticMaterial>& USkeletalMesh::GetStaticMaterials() const
{
	return StaticMaterials;
}

TArray<FStaticMaterial>& USkeletalMesh::GetStaticMaterialsMutable()
{
	return StaticMaterials;
}

FStaticMaterial* USkeletalMesh::GetStaticMaterial(int32 MaterialIndex)
{
	return (MaterialIndex >= 0 && MaterialIndex < static_cast<int32>(StaticMaterials.size()))
		? &StaticMaterials[MaterialIndex]
		: nullptr;
}

bool USkeletalMesh::SetStaticMaterialInterface(int32 MaterialIndex, UMaterial* InMaterial)
{
	FStaticMaterial* MaterialSlot = GetStaticMaterial(MaterialIndex);
	if (!MaterialSlot)
	{
		return false;
	}

	MaterialSlot->MaterialInterface = InMaterial;
	return true;
}
