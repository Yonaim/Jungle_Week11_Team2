#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include "Engine/Object/Object.h"
#include "Render/Resource/Buffer.h"
#include "Serialization/Archive.h"
#include "Engine/Object/FName.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include <memory>
#include <algorithm>

// 렌더/임포트 공용 정점 포맷(정적 메시, 스키닝 결과 버퍼 공용)
struct FNormalVertex
{
	FVector pos;
	FVector normal;
	FVector4 color;
	FVector2 tex;
	FVector4 tangent;
};

// 하나의 렌더 섹션(대개 서브메시/머티리얼 슬롯 단위)
struct FStaticMeshSection
{
	int32 MaterialIndex = -1; // UStaticMesh::StaticMaterials 인덱스 캐시
	FString MaterialSlotName;
	uint32 FirstIndex;
	uint32 NumTriangles;

	friend FArchive& operator<<(FArchive& Ar, FStaticMeshSection& Section)
	{
		Ar << Section.MaterialSlotName << Section.FirstIndex << Section.NumTriangles;
		return Ar;
	}
};

// 에셋 직렬화용 머티리얼 슬롯 정보.
// 메시 섹션은 SlotName으로 머티리얼과 연결되고, 실제 머티리얼은 경로 기반으로 복원된다.
struct FStaticMaterial
{
	UMaterial* MaterialInterface;
	FString MaterialSlotName = "None";

	friend FArchive& operator<<(FArchive& Ar, FStaticMaterial& Mat)
	{
		Ar << Mat.MaterialSlotName;

		FString JsonPath;
		if (Ar.IsSaving() && Mat.MaterialInterface)
		{
			JsonPath = Mat.MaterialInterface->GetAssetPathFileName();
		}
		Ar << JsonPath;

		if (Ar.IsLoading())
		{
			if (!JsonPath.empty())
			{
				Mat.MaterialInterface = FMaterialManager::Get().GetOrCreateMaterial(JsonPath);
			}
			else
			{
				Mat.MaterialInterface = nullptr;
			}
		}

		return Ar;
	}
};

// StaticMesh Cooked Data 본체.
// 정점/인덱스/섹션은 에셋 원본 데이터이며, RenderBuffer는 이 데이터에서 생성되는 GPU 리소스 캐시다.
struct FStaticMesh
{
	FString PathFileName;
	TArray<FNormalVertex> Vertices;
	TArray<uint32> Indices;
	TArray<FStaticMeshSection> Sections;

	std::unique_ptr<FMeshBuffer> RenderBuffer;

	// 로컬 바운드 캐시: 피킹/컬링/AABB 갱신 경로에서 재사용한다.
	FVector BoundsCenter = FVector(0, 0, 0);
	FVector BoundsExtent = FVector(0, 0, 0);
	bool    bBoundsValid = false;

	void CacheBounds()
	{
		bBoundsValid = false;
		if (Vertices.empty()) return;

		FVector LocalMin = Vertices[0].pos;
		FVector LocalMax = Vertices[0].pos;
		for (const FNormalVertex& V : Vertices)
		{
			LocalMin.X = (std::min)(LocalMin.X, V.pos.X);
			LocalMin.Y = (std::min)(LocalMin.Y, V.pos.Y);
			LocalMin.Z = (std::min)(LocalMin.Z, V.pos.Z);
			LocalMax.X = (std::max)(LocalMax.X, V.pos.X);
			LocalMax.Y = (std::max)(LocalMax.Y, V.pos.Y);
			LocalMax.Z = (std::max)(LocalMax.Z, V.pos.Z);
		}

		BoundsCenter = (LocalMin + LocalMax) * 0.5f;
		BoundsExtent = (LocalMax - LocalMin) * 0.5f;
		bBoundsValid = true;
	}

	void Serialize(FArchive& Ar)
	{
		Ar << PathFileName;
		Ar << Vertices;
		Ar << Indices;
		Ar << Sections;
	}
};
