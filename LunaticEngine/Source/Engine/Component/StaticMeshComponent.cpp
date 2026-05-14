#include "PCH/LunaticPCH.h"
#include "Component/StaticMeshComponent.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include "Object/ObjectFactory.h"
#include "Core/PropertyTypes.h"
#include "Collision/RayUtils.h"
#include "Mesh/StaticMeshCommon.h"
#include "Engine/Runtime/Engine.h"
#include "Materials/MaterialManager.h"
#include "Render/Shader/ShaderManager.h"
#include "Texture/Texture2D.h"
#include "Render/Proxy/StaticMeshSceneProxy.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Serialization/Archive.h"
#include "GameFramework/AActor.h"

IMPLEMENT_CLASS(UStaticMeshComponent, UMeshComponent)

namespace
{
	int32 GetRequiredMaterialSlotCount(const UStaticMesh* StaticMesh)
	{
		if (!StaticMesh)
		{
			return 0;
		}

		const TArray<FStaticMaterial>& DefaultMaterials = StaticMesh->GetStaticMaterials();
		if (!DefaultMaterials.empty())
		{
			return static_cast<int32>(DefaultMaterials.size());
		}

		const FStaticMesh* MeshAsset = StaticMesh->GetStaticMeshAsset();
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

	void EnsureMaterialSlotStorage(UStaticMesh* StaticMesh, TArray<UMaterial*>& OverrideMaterials, TArray<FMaterialSlot>& MaterialSlots)
	{
		const int32 RequiredSlotCount = GetRequiredMaterialSlotCount(StaticMesh);
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

		const TArray<FStaticMaterial>& DefaultMaterials = StaticMesh ? StaticMesh->GetStaticMaterials() : TArray<FStaticMaterial>{};

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

FPrimitiveSceneProxy* UStaticMeshComponent::CreateSceneProxy()
{
	// 렌더 제출 경계:
	// 이후 드로우 커맨드는 컴포넌트가 아니라 FStaticMeshSceneProxy를 통해 수집된다.
	return new FStaticMeshSceneProxy(this);
}

void UStaticMeshComponent::SetStaticMesh(UStaticMesh* InMesh)
{
	StaticMesh = InMesh;
	if (InMesh)
	{
		StaticMeshPath = InMesh->GetAssetPathFileName();
		OverrideMaterials.clear();
		MaterialSlots.clear();
		EnsureMaterialSlotStorage(StaticMesh, OverrideMaterials, MaterialSlots);
	}
	else
	{
		StaticMeshPath = "None";
		OverrideMaterials.clear();
		MaterialSlots.clear();
	}
	CacheLocalBounds();
	MarkRenderStateDirty();
	MarkWorldBoundsDirty();
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->SyncEditorBillboardVisibility();
	}
}

void UStaticMeshComponent::CacheLocalBounds()
{
	bHasValidBounds = false;
	if (!StaticMesh) return;
	FStaticMesh* Asset = StaticMesh->GetStaticMeshAsset();
	if (!Asset || Asset->Vertices.empty()) return;

	// FStaticMesh에 이미 계산된 바운드가 있으면 그대로 사용
	if (!Asset->bBoundsValid)
	{
		Asset->CacheBounds();
	}

	CachedLocalCenter = Asset->BoundsCenter;
	CachedLocalExtent = Asset->BoundsExtent;
	bHasValidBounds = Asset->bBoundsValid;
}

UStaticMesh* UStaticMeshComponent::GetStaticMesh() const
{
	return StaticMesh;
}

void UStaticMeshComponent::EnsureMaterialSlotsForEditing()
{
	EnsureMaterialSlotStorage(StaticMesh, OverrideMaterials, MaterialSlots);
}

void UStaticMeshComponent::SetMaterial(int32 ElementIndex, UMaterial* InMaterial)
{
	if (ElementIndex < 0)
	{
		return;
	}

	const int32 RequiredSlotCount = GetRequiredMaterialSlotCount(StaticMesh);
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

UMaterial* UStaticMeshComponent::GetMaterial(int32 ElementIndex) const
{
	if (ElementIndex >= 0 && ElementIndex < OverrideMaterials.size())
	{
		return OverrideMaterials[ElementIndex];
	}
	return nullptr;
}

FMaterialSlot* UStaticMeshComponent::GetMaterialSlot(int32 ElementIndex)
{
	EnsureMaterialSlotsForEditing();
	return (ElementIndex >= 0 && ElementIndex < static_cast<int32>(MaterialSlots.size()))
		? &MaterialSlots[ElementIndex]
		: nullptr;
}

const FMaterialSlot* UStaticMeshComponent::GetMaterialSlot(int32 ElementIndex) const
{
	return (ElementIndex >= 0 && ElementIndex < static_cast<int32>(MaterialSlots.size()))
		? &MaterialSlots[ElementIndex]
		: nullptr;
}

FMeshBuffer* UStaticMeshComponent::GetMeshBuffer() const
{
	if (!StaticMesh) return nullptr;
	FStaticMesh* Asset = StaticMesh->GetStaticMeshAsset();
	if (!Asset || !Asset->RenderBuffer) return nullptr;
	return Asset->RenderBuffer.get();
}

FMeshDataView UStaticMeshComponent::GetMeshDataView() const
{
	if (!StaticMesh) return {};
	FStaticMesh* Asset = StaticMesh->GetStaticMeshAsset();
	if (!Asset || Asset->Vertices.empty()) return {};

	FMeshDataView View;
	View.VertexData  = Asset->Vertices.data();
	View.VertexCount = (uint32)Asset->Vertices.size();
	View.Stride      = sizeof(FNormalVertex);
	View.IndexData   = Asset->Indices.data();
	View.IndexCount  = (uint32)Asset->Indices.size();
	return View;
}

void UStaticMeshComponent::UpdateWorldAABB() const
{
	if (!bHasValidBounds)
	{
		UPrimitiveComponent::UpdateWorldAABB();
		return;
	}

	FVector WorldCenter = CachedWorldMatrix.TransformPositionWithW(CachedLocalCenter);

	float Ex = std::abs(CachedWorldMatrix.M[0][0]) * CachedLocalExtent.X
		+ std::abs(CachedWorldMatrix.M[1][0]) * CachedLocalExtent.Y
		+ std::abs(CachedWorldMatrix.M[2][0]) * CachedLocalExtent.Z;
	float Ey = std::abs(CachedWorldMatrix.M[0][1]) * CachedLocalExtent.X
		+ std::abs(CachedWorldMatrix.M[1][1]) * CachedLocalExtent.Y
		+ std::abs(CachedWorldMatrix.M[2][1]) * CachedLocalExtent.Z;
	float Ez = std::abs(CachedWorldMatrix.M[0][2]) * CachedLocalExtent.X
		+ std::abs(CachedWorldMatrix.M[1][2]) * CachedLocalExtent.Y
		+ std::abs(CachedWorldMatrix.M[2][2]) * CachedLocalExtent.Z;

	WorldAABBMinLocation = WorldCenter - FVector(Ex, Ey, Ez);
	WorldAABBMaxLocation = WorldCenter + FVector(Ex, Ey, Ez);
	bWorldAABBDirty = false;
	bHasValidWorldAABB = true;
}

bool UStaticMeshComponent::LineTraceComponent(const FRay& Ray, FRayHitResult& OutHitResult)
{
	const FMatrix& WorldMatrix = GetWorldMatrix();
	const FMatrix& WorldInverse = GetWorldInverseMatrix();
	return LineTraceStaticMeshFast(Ray, WorldMatrix, WorldInverse, OutHitResult);
}

bool UStaticMeshComponent::LineTraceStaticMeshFast(
	const FRay& Ray,
	const FMatrix& WorldMatrix,
	const FMatrix& WorldInverse,
	FRayHitResult& OutHitResult)
{
	if (!StaticMesh) return false;

	FVector LocalOrigin = WorldInverse.TransformPositionWithW(Ray.Origin);
	FVector LocalDirection = WorldInverse.TransformVector(Ray.Direction);
	LocalDirection.Normalize();

	// 월드 BVH가 후보 Primitive를 추린 뒤,
	// 여기서는 메시 로컬 BVH로 삼각형 정밀 판정만 수행한다.
	if (StaticMesh->RaycastMeshTrianglesWithBVHLocal(LocalOrigin, LocalDirection, OutHitResult))
	{
		const FVector LocalHitPoint = LocalOrigin + LocalDirection * OutHitResult.Distance;
		const FVector WorldHitPoint = WorldMatrix.TransformPositionWithW(LocalHitPoint);
		OutHitResult.Distance = FVector::Distance(Ray.Origin, WorldHitPoint);
		OutHitResult.HitComponent = this;
		return true;
	}

	return false;
}

// FArchive 기반 직렬화 — 복제 왕복용. 자산은 경로로만 들고, 실제 로드는 PostDuplicate에서.
static FArchive& operator<<(FArchive& Ar, FMaterialSlot& Slot)
{
	Ar << Slot.Path;
	return Ar;
}

void UStaticMeshComponent::Serialize(FArchive& Ar)
{
	UMeshComponent::Serialize(Ar);
	Ar << StaticMeshPath;
	Ar << MaterialSlots;
}

void UStaticMeshComponent::PostDuplicate()
{
	UMeshComponent::PostDuplicate();

	// 메시 에셋 재로딩
	if (!StaticMeshPath.empty() && StaticMeshPath != "None")
	{
		ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
		UStaticMesh* Loaded = FMeshAssetManager::LoadStaticMesh(StaticMeshPath, Device);
		if (Loaded)
		{
			// SetStaticMesh는 MaterialSlots를 덮어쓰므로, 직렬화된 슬롯 정보를 백업·복원한다.
			TArray<FMaterialSlot> SavedSlots = MaterialSlots;
			SetStaticMesh(Loaded);

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

	CacheLocalBounds();
	MarkRenderStateDirty();
	MarkWorldBoundsDirty();
}

void UStaticMeshComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	EnsureMaterialSlotsForEditing();

	UPrimitiveComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Static Mesh", EPropertyType::StaticMeshRef, &StaticMeshPath });

	for (int32 i = 0; i < (int32)MaterialSlots.size(); ++i)
	{
		FPropertyDescriptor Desc;
		Desc.Name = "Element " + std::to_string(i);
		Desc.Type = EPropertyType::MaterialSlot;
		Desc.ValuePtr = &MaterialSlots[i];
		OutProps.push_back(Desc);
	}
}

void UStaticMeshComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Static Mesh") == 0)
	{
		if (StaticMeshPath.empty() || StaticMeshPath == "None")
		{
			SetStaticMesh(nullptr);
		}
		else
		{
			const FString RequestedMeshPath = StaticMeshPath;
			if (!IsUAssetReferencePath(RequestedMeshPath))
			{
				SetStaticMesh(nullptr);
				StaticMeshPath = "None";
			}
			else
			{
				ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
				UStaticMesh* Loaded = FMeshAssetManager::LoadStaticMeshAssetFile(RequestedMeshPath, Device);
				SetStaticMesh(Loaded);
				if (Loaded)
				{
					StaticMeshPath = RequestedMeshPath;
				}
			}
		}
		CacheLocalBounds();
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
