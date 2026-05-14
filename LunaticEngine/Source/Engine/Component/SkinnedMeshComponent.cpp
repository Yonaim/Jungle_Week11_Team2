#include "PCH/LunaticPCH.h"
#include "SkinnedMeshComponent.h"
#include "Render/Proxy/SkeletalMeshProxy.h"
#include "Mesh/MeshAssetManager.h"
#include "Engine/Runtime/Engine.h"
#include "Serialization/Archive.h"
#include <algorithm>
#include <cmath>
#include <cctype>

IMPLEMENT_CLASS(USkinnedMeshComponent, UMeshComponent)

namespace
{
	bool IsFiniteVertexPosition(const FVector& Value)
	{
		return std::isfinite(Value.X) && std::isfinite(Value.Y) && std::isfinite(Value.Z);
	}

	int32 GetRequiredMaterialSlotCount(const USkeletalMesh* SkeletalMesh)
	{
		if (!SkeletalMesh)
		{
			return 0;
		}

		const TArray<FStaticMaterial>& DefaultMaterials = SkeletalMesh->GetStaticMaterials();
		if (!DefaultMaterials.empty())
		{
			return static_cast<int32>(DefaultMaterials.size());
		}

		const FSkeletalMesh* MeshAsset = SkeletalMesh->GetSkeletalMeshAsset();
		if (MeshAsset && (!MeshAsset->Sections.empty() || !MeshAsset->Indices.empty()))
		{
			return 1;
		}

		return 0;
	}


	bool IsUAssetReferencePath(const FString& Path)
	{
		if (Path.empty() || Path == "None")
		{
			return true;
		}

		FString LowerPath = Path;
		std::transform(LowerPath.begin(), LowerPath.end(), LowerPath.begin(), [](unsigned char Ch)
		{
			return static_cast<char>(std::tolower(Ch));
		});
		const FString Extension = ".uasset";
		return LowerPath.size() >= Extension.size() &&
			LowerPath.compare(LowerPath.size() - Extension.size(), Extension.size(), Extension) == 0;
	}

	void EnsureMaterialSlotStorage(USkeletalMesh* SkeletalMesh, TArray<UMaterial*>& OverrideMaterials, TArray<FMaterialSlot>& MaterialSlots)
	{
		const int32 RequiredSlotCount = GetRequiredMaterialSlotCount(SkeletalMesh);
		if (RequiredSlotCount <= 0)
		{
			return;
		}

		const int32 PreviousOverrideCount = static_cast<int32>(OverrideMaterials.size());
		const int32 PreviousSlotCount = static_cast<int32>(MaterialSlots.size());
		if (PreviousOverrideCount >= RequiredSlotCount && PreviousSlotCount >= RequiredSlotCount)
		{
			return;
		}

		const TArray<FStaticMaterial>& DefaultMaterials = SkeletalMesh ? SkeletalMesh->GetStaticMaterials() : TArray<FStaticMaterial>{};

		OverrideMaterials.resize(RequiredSlotCount, nullptr);
		MaterialSlots.resize(RequiredSlotCount);

		for (int32 i = 0; i < RequiredSlotCount; ++i)
		{
			if (i >= PreviousOverrideCount)
			{
				if (i < static_cast<int32>(DefaultMaterials.size()))
				{
					OverrideMaterials[i] = DefaultMaterials[i].MaterialInterface;
				}
				else
				{
					OverrideMaterials[i] = FMaterialManager::Get().GetOrCreateMaterial("None");
				}
			}

			if (i >= PreviousSlotCount || MaterialSlots[i].Path.empty())
			{
				MaterialSlots[i].Path = OverrideMaterials[i]
					? OverrideMaterials[i]->GetAssetPathFileName()
					: "None";
			}
		}
	}
}

void USkinnedMeshComponent::SetSkeletalMesh(USkeletalMesh* Mesh)
{
	// 컴포넌트는 애셋 경로를 직렬화용 식별자로만 저장한다.
	// 실제 지오메트리/스킨 데이터 소유권은 USkeletalMesh 내부에 있다.
	SkeletalMesh = Mesh;
	if (Mesh && Mesh->GetSkeletalMeshAsset())
	{
		SkeletalMeshPath = Mesh->GetSkeletalMeshAsset()->PathFileName;
		OverrideMaterials.clear();
		MaterialSlots.clear();
		EnsureMaterialSlotStorage(SkeletalMesh, OverrideMaterials, MaterialSlots);
	}
	else
	{
		SkeletalMeshPath = "None";
		OverrideMaterials.clear();
		MaterialSlots.clear();
	}
	MarkRenderStateDirty();
	MarkWorldBoundsDirty();
}

USkeletalMesh* USkinnedMeshComponent::GetSkeletalMesh() const
{
	return SkeletalMesh;
}

void USkinnedMeshComponent::SetMaterial(int32 ElementIndex, UMaterial* InMaterial)
{
	if (ElementIndex < 0)
	{
		return;
	}

	const int32 RequiredSlotCount = GetRequiredMaterialSlotCount(SkeletalMesh);
	if (ElementIndex >= static_cast<int32>(OverrideMaterials.size()) && ElementIndex < RequiredSlotCount)
	{
		const int32 NewSlotCount = std::max(RequiredSlotCount, ElementIndex + 1);
		OverrideMaterials.resize(NewSlotCount, nullptr);
		MaterialSlots.resize(NewSlotCount);

		for (int32 SlotIndex = 0; SlotIndex < NewSlotCount; ++SlotIndex)
		{
			if (MaterialSlots[SlotIndex].Path.empty())
			{
				MaterialSlots[SlotIndex].Path = "None";
			}
		}
	}

	if (ElementIndex < static_cast<int32>(OverrideMaterials.size()))
	{
		OverrideMaterials[ElementIndex] = InMaterial;

		// MaterialSlots 동기화 — 씬 저장 시 경로가 올바르게 직렬화되도록
		if (ElementIndex < static_cast<int32>(MaterialSlots.size()))
		{
			MaterialSlots[ElementIndex].Path = InMaterial
				? InMaterial->GetAssetPathFileName()
				: "None";
		}

		// 프록시에 Material dirty 전파
		MarkProxyDirty(EDirtyFlag::Material);
	}
}

UMaterial* USkinnedMeshComponent::GetMaterial(int32 ElementIndex) const
{
	if (ElementIndex >= 0 && ElementIndex < OverrideMaterials.size())
	{
		return OverrideMaterials[ElementIndex];
	}
	return nullptr;
}

void USkinnedMeshComponent::EnsureMaterialSlotsForEditing()
{
	EnsureMaterialSlotStorage(SkeletalMesh, OverrideMaterials, MaterialSlots);
}

FTransform USkinnedMeshComponent::GetBoneWorldTransform(int32 BoneIndex) const
{
	const TArray<FMatrix>& ComponentSpaceTransforms = CurrentPose.ComponentTransforms;
	if (BoneIndex < 0 || BoneIndex >= (int32)ComponentSpaceTransforms.size())
		return FTransform();

	// 본의 컴포넌트 공간 행렬에 컴포넌트 월드 행렬을 곱해
	// 최종 월드 공간 본 변환으로 변환한다.
	FMatrix BoneWorldMatrix = ComponentSpaceTransforms[BoneIndex] * GetWorldMatrix();
	return FTransform(
		BoneWorldMatrix.GetLocation(),
		BoneWorldMatrix.ToQuat(),
		BoneWorldMatrix.GetScale()
	);
}

FTransform USkinnedMeshComponent::GetBoneWorldTransformByName(const FString& BoneName) const
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset())
	{
		return FTransform();
	}

	const TArray<FBoneInfo>& Bones = SkeletalMesh->GetSkeletalMeshAsset()->Bones;
	for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Bones.size()); ++BoneIndex)
	{
		if (Bones[BoneIndex].Name == BoneName)
		{
			return GetBoneWorldTransform(BoneIndex);
		}
	}

	return FTransform();
}

int32 USkinnedMeshComponent::GetBoneCount() const
{
	return (int32)CurrentPose.LocalTransforms.size();
}

FPrimitiveSceneProxy* USkinnedMeshComponent::CreateSceneProxy()
{
	// 렌더 제출 경계:
	// 컴포넌트 런타임 상태(CurrentPose/SkinBuffer)는 프록시가 소비하고,
	// DrawCommandBuilder는 프록시의 버퍼/섹션 정보만 사용한다.
	return new FSkeletalMeshProxy(this);
}

void USkinnedMeshComponent::UpdateWorldAABB() const
{
	// 스키닝 메시는 포즈에 따라 정점 위치가 매 프레임 변하므로,
	// 정적 애셋 바운드 대신 CPU Skinning 결과 버퍼로 AABB를 계산한다.
	TArray<FNormalVertex>* Verts = const_cast<USkinnedMeshComponent*>(this)->GetCPUSkinnedVertices();
	if (!Verts || Verts->empty())
	{
		UPrimitiveComponent::UpdateWorldAABB();
		return;
	}

	bool bFoundFiniteVertex = false;
	FVector LocalMin = FVector::ZeroVector;
	FVector LocalMax = FVector::ZeroVector;
	for (const FNormalVertex& V : *Verts)
	{
		if (!IsFiniteVertexPosition(V.pos))
		{
			continue;
		}

		if (!bFoundFiniteVertex)
		{
			LocalMin = V.pos;
			LocalMax = V.pos;
			bFoundFiniteVertex = true;
			continue;
		}

		LocalMin.X = std::min(LocalMin.X, V.pos.X);
		LocalMin.Y = std::min(LocalMin.Y, V.pos.Y);
		LocalMin.Z = std::min(LocalMin.Z, V.pos.Z);
		LocalMax.X = std::max(LocalMax.X, V.pos.X);
		LocalMax.Y = std::max(LocalMax.Y, V.pos.Y);
		LocalMax.Z = std::max(LocalMax.Z, V.pos.Z);
	}

	if (!bFoundFiniteVertex)
	{
		UPrimitiveComponent::UpdateWorldAABB();
		return;
	}

	FVector LocalCenter = (LocalMin + LocalMax) * 0.5f;
	FVector LocalExtent = (LocalMax - LocalMin) * 0.5f;

	const FMatrix& W = GetWorldMatrix();
	FVector WorldCenter = W.TransformPositionWithW(LocalCenter);

	float Ex = std::abs(W.M[0][0]) * LocalExtent.X + std::abs(W.M[1][0]) * LocalExtent.Y + std::abs(W.M[2][0]) * LocalExtent.Z;
	float Ey = std::abs(W.M[0][1]) * LocalExtent.X + std::abs(W.M[1][1]) * LocalExtent.Y + std::abs(W.M[2][1]) * LocalExtent.Z;
	float Ez = std::abs(W.M[0][2]) * LocalExtent.X + std::abs(W.M[1][2]) * LocalExtent.Y + std::abs(W.M[2][2]) * LocalExtent.Z;

	WorldAABBMinLocation = WorldCenter - FVector(Ex, Ey, Ez);
	WorldAABBMaxLocation = WorldCenter + FVector(Ex, Ey, Ez);
	bWorldAABBDirty = false;
	bHasValidWorldAABB = true;
}

void USkinnedMeshComponent::InitBoneTransform()
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset())
	{
		CurrentPose.Reset();
		return;
	}

	CurrentPose.InitializeFromBindPose(*SkeletalMesh->GetSkeletalMeshAsset());
}

void USkinnedMeshComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	EnsureMaterialSlotsForEditing();

	UPrimitiveComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Skeletal Mesh", EPropertyType::SkeletalMeshRef, &SkeletalMeshPath });

	for (int32 i = 0; i < (int32)MaterialSlots.size(); ++i)
	{
		FPropertyDescriptor Desc;
		Desc.Name = "Element " + std::to_string(i);
		Desc.Type = EPropertyType::MaterialSlot;
		Desc.ValuePtr = &MaterialSlots[i];
		OutProps.push_back(Desc);
	}
}

void USkinnedMeshComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Skeletal Mesh") == 0)
	{
		if (SkeletalMeshPath.empty() || SkeletalMeshPath == "None")
		{
			SetSkeletalMesh(nullptr);
		}
		else
		{
			const FString RequestedMeshPath = SkeletalMeshPath;
			if (!IsUAssetReferencePath(RequestedMeshPath))
			{
				SetSkeletalMesh(nullptr);
				SkeletalMeshPath = "None";
			}
			else
			{
				ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
				USkeletalMesh* Loaded = FMeshAssetManager::LoadSkeletalMeshAssetFile(RequestedMeshPath, Device);
				SetSkeletalMesh(Loaded);
				if (Loaded)
				{
					SkeletalMeshPath = RequestedMeshPath;
				}
			}
		}
		MarkWorldBoundsDirty();
	}

	if (strncmp(PropertyName, "Element ", 8) == 0)
	{
		// "Element 0"에서 8번째 인덱스부터 시작하는 숫자를 정수로 변환
		int32 Index = atoi(&PropertyName[8]);

		// 인덱스 범위 유효성 검사
		if (Index >= 0 && Index < (int32)MaterialSlots.size())
		{
			FString NewMatPath = MaterialSlots[Index].Path;

			if (NewMatPath == "None" || NewMatPath.empty())
			{
				SetMaterial(Index, nullptr);
			}
			else
			{
				UMaterial* LoadedMat = FMaterialManager::Get().GetOrCreateMaterial(NewMatPath);
				if (LoadedMat)
				{
					SetMaterial(Index, LoadedMat);
				}
			}
		}
	}
}

// FArchive 기반 직렬화 — 복제 왕복용. 자산은 경로로만 들고, 실제 로드는 PostDuplicate에서.
static FArchive& operator<<(FArchive& Ar, FMaterialSlot& Slot)
{
	Ar << Slot.Path;
	return Ar;
}

void USkinnedMeshComponent::Serialize(FArchive& Ar)
{
	UMeshComponent::Serialize(Ar);
	Ar << SkeletalMeshPath;
	Ar << MaterialSlots;
}

void USkinnedMeshComponent::PostDuplicate()
{
	UMeshComponent::PostDuplicate();

	if (!SkeletalMeshPath.empty() && SkeletalMeshPath != "None")
	{
		const FString RequestedMeshPath = SkeletalMeshPath;
		ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
		USkeletalMesh* Loaded = FMeshAssetManager::LoadSkeletalMesh(SkeletalMeshPath, Device);
		if (Loaded)
		{
			// SetSkeletalMesh는 MaterialSlots를 덮어쓰므로, 직렬화된 슬롯 정보를 백업·복원한다.
			TArray<FMaterialSlot> SavedSlots = MaterialSlots;
			SetSkeletalMesh(Loaded);
			SkeletalMeshPath = RequestedMeshPath;

			// Override material 재로딩
			for (int32 i = 0; i < (int32)MaterialSlots.size() && i < (int32)SavedSlots.size(); ++i)
			{
				MaterialSlots[i] = SavedSlots[i];
				const FString& MatPath = MaterialSlots[i].Path;
				if (MatPath.empty() || MatPath == "None")
				{
					OverrideMaterials[i] = nullptr;
				}
				else
				{
					UMaterial* LoadedMat = FMaterialManager::Get().GetOrCreateMaterial(MatPath);
					OverrideMaterials[i] = LoadedMat;
				}
			}
		}
	}

	MarkRenderStateDirty();
	MarkWorldBoundsDirty();
}
