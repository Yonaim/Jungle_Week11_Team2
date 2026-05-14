#pragma once

#include "Object/Object.h"
#include "Engine/Mesh/SkeletalMeshCommon.h"

// USkeletalMesh:
// SkeletalMesh 런타임 애셋 UObject 래퍼.
// 실제 기하/스킨/본 데이터는 FSkeletalMesh(Cooked Data)에 보관되고,
// 이 클래스는 소유권/직렬화/머티리얼 슬롯 연계를 담당한다.
class USkeletalMesh : public UObject
{
public:
	DECLARE_CLASS(USkeletalMesh, UObject)

	USkeletalMesh() = default;
	~USkeletalMesh() override;

	void Serialize(FArchive& Ar);

	bool IsValid() const;

	int32 GetBoneCount() const;
	int32 GetVertexCount(int32 LODIndex = 0) const;
	int32 GetIndexCount(int32 LODIndex = 0) const;

	void SetSkeletalMeshAsset(FSkeletalMesh* InMesh);
	FSkeletalMesh* GetSkeletalMeshAsset() const;

	const TArray<FBoneInfo>& GetBones() const;
	const FBoneInfo* GetBoneInfo(int32 BoneIndex) const;
	int32 GetParentBoneIndex(int32 BoneIndex) const;
	const char* GetBoneName(int32 BoneIndex) const;

	const TArray<TArray<int32>>& GetBoneChildren() const;
	const TArray<int32>& GetRootBoneIndices() const;

	void SetStaticMaterials(TArray<FStaticMaterial>&& InMaterials);
	const TArray<FStaticMaterial>& GetStaticMaterials() const;
	TArray<FStaticMaterial>& GetStaticMaterialsMutable();
	FStaticMaterial* GetStaticMaterial(int32 MaterialIndex);
	bool SetStaticMaterialInterface(int32 MaterialIndex, UMaterial* InMaterial);

private:
	// 애셋 본체 소유 포인터. 컴포넌트는 이 데이터를 참조만 한다.
	FSkeletalMesh* SkeletalMesh = nullptr;
	TArray<FStaticMaterial> StaticMaterials;
};
