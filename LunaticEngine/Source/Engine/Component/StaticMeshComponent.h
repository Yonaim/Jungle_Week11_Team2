#pragma once

#include "Component/MeshComponent.h"
#include "Core/PropertyTypes.h"
#include "Mesh/MeshAssetManager.h"
#include "Mesh/StaticMesh.h"

class UMaterial;
class FPrimitiveSceneProxy;

namespace json { class JSON; }

// UStaticMeshComponent:
// 월드에 배치된 StaticMesh 인스턴스 컴포넌트.
// - 애셋(UStaticMesh) 참조
// - 인스턴스별 머티리얼 오버라이드
// - 프록시 생성(CreateSceneProxy)으로 렌더 제출 경계 연결
class UStaticMeshComponent : public UMeshComponent
{
public:
	DECLARE_CLASS(UStaticMeshComponent, UMeshComponent)

	UStaticMeshComponent() = default;
	~UStaticMeshComponent() override = default;

	FMeshBuffer* GetMeshBuffer() const override;
	FMeshDataView GetMeshDataView() const override;
	bool LineTraceComponent(const FRay& Ray, FRayHitResult& OutHitResult) override;
	bool LineTraceStaticMeshFast(const FRay& Ray, const FMatrix& WorldMatrix, const FMatrix& WorldInverse, FRayHitResult& OutHitResult);
	void UpdateWorldAABB() const override;

	// 게임 스레드의 컴포넌트 데이터를 렌더 스레드 프록시로 미러링하는 경계 지점.
	FPrimitiveSceneProxy* CreateSceneProxy() override;

	void SetStaticMesh(UStaticMesh* InMesh);
	UStaticMesh* GetStaticMesh() const;

	void SetMaterial(int32 ElementIndex, UMaterial* InMaterial);
	UMaterial* GetMaterial(int32 ElementIndex) const;
	const TArray<UMaterial*>& GetOverrideMaterials() const { return OverrideMaterials; }
	void EnsureMaterialSlotsForEditing();
	int32 GetMaterialSlotCount() const { return static_cast<int32>(MaterialSlots.size()); }
	FMaterialSlot* GetMaterialSlot(int32 ElementIndex);
	const FMaterialSlot* GetMaterialSlot(int32 ElementIndex) const;

	void Serialize(FArchive& Ar) override;
	void PostDuplicate() override;

	// Property Editor 지원
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

	const FString& GetStaticMeshPath() const { return StaticMeshPath; }

private:
	void CacheLocalBounds();

	// 애셋 소유권은 UStaticMesh에 있고, 컴포넌트는 참조만 유지한다.
	UStaticMesh* StaticMesh = nullptr;
	FString StaticMeshPath = "None";
	// 인스턴스별 머티리얼 오버라이드(애셋 기본 슬롯을 덮어씀).
	TArray<UMaterial*> OverrideMaterials;
	// 직렬화/에디터 바인딩용 머티리얼 슬롯 상태.
	TArray<FMaterialSlot> MaterialSlots;

	FVector CachedLocalCenter = { 0, 0, 0 };
	FVector CachedLocalExtent = { 0.5f, 0.5f, 0.5f };
	bool bHasValidBounds = false;
};
