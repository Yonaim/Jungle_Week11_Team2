#include "PCH/LunaticPCH.h"
#include "GameFramework/StaticMeshActor.h"
#include "Object/ObjectFactory.h"
#include "Engine/Runtime/Engine.h"
#include "Component/StaticMeshComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/SubUVComponent.h"
#include "Materials/MaterialManager.h"
#include "Resource/ResourceManager.h"
#include "Engine/Platform/Paths.h"

#include <filesystem>
#include <algorithm>
#include <cctype>

IMPLEMENT_CLASS(AStaticMeshActor, AActor)

namespace
{
	FString ToLowerAsciiCopy(FString Value)
	{
		std::transform(Value.begin(), Value.end(), Value.begin(), [](unsigned char Character)
		{
			return static_cast<char>(std::tolower(Character));
		});
		return Value;
	}

	bool DoesProjectRelativePathExist(const FString& Path)
	{
		if (Path.empty())
		{
			return false;
		}

		std::filesystem::path Candidate(FPaths::ToWide(Path));
		if (!Candidate.is_absolute())
		{
			Candidate = std::filesystem::path(FPaths::RootDir()) / Candidate;
		}
		return std::filesystem::exists(Candidate.lexically_normal());
	}

	FString GetBasicShapeFallbackObjPath(const FString& MeshPath)
	{
		struct FShapeFallback
		{
			const char* AssetStem;
			const char* ObjName;
		};

		static constexpr FShapeFallback ShapeFallbacks[] = {
			{ "sm_cone", "cone.obj" },
			{ "sm_cube", "cube.obj" },
			{ "sm_cylinder", "cylinder.obj" },
			{ "sm_plane", "plane.obj" },
			{ "sm_sphere_lowpoly", "sphere_lowpoly.obj" },
			{ "sm_sphere", "sphere.obj" },
		};

		const FString LowerMeshPath = ToLowerAsciiCopy(MeshPath);

		for (const FShapeFallback& Fallback : ShapeFallbacks)
		{
			if (LowerMeshPath.find(Fallback.AssetStem) == FString::npos && LowerMeshPath.find(Fallback.ObjName) == FString::npos)
			{
				continue;
			}

			const FString Candidate = FPaths::ToUtf8((std::filesystem::path(FPaths::EngineBasicShapeSourceDir()) / Fallback.ObjName).generic_wstring());
			if (DoesProjectRelativePathExist(Candidate))
			{
				return Candidate;
			}
		}

		return MeshPath;
	}
}

void AStaticMeshActor::InitDefaultComponents(const FString& UStaticMeshFileName)
{
	StaticMeshComponent = AddComponent<UStaticMeshComponent>();
	StaticMeshComponent->SetCanDeleteFromDetails(false);
	SetRootComponent(StaticMeshComponent);

	if (!UStaticMeshFileName.empty() && UStaticMeshFileName != "None")
	{
		const FString ResolvedMeshPath = DoesProjectRelativePathExist(UStaticMeshFileName)
			? UStaticMeshFileName
			: GetBasicShapeFallbackObjPath(UStaticMeshFileName);

		ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
		UStaticMesh* Asset = FMeshAssetManager::LoadStaticMesh(ResolvedMeshPath, Device);
		StaticMeshComponent->SetStaticMesh(Asset);

		if (Asset && IsBasicShapeAssetPath(ResolvedMeshPath))
		{
			const FString DefaultShapeMaterialPath = FResourceManager::Get().ResolvePath(FName("Default.Material.BasicShape"));
			if (UMaterial* DefaultShapeMaterial = FMaterialManager::Get().GetOrCreateMaterial(DefaultShapeMaterialPath))
			{
				int32 MaterialCount = static_cast<int32>(Asset->GetStaticMaterials().size());
				if (MaterialCount == 0 && Asset->GetStaticMeshAsset() &&
					(!Asset->GetStaticMeshAsset()->Sections.empty() || !Asset->GetStaticMeshAsset()->Indices.empty()))
				{
					MaterialCount = 1;
				}
				for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
				{
					StaticMeshComponent->SetMaterial(MaterialIndex, DefaultShapeMaterial);
				}
			}
		}
	}
	else
	{
		StaticMeshComponent->SetStaticMesh(nullptr);
	}

	// UUID 텍스트 표시
	//TextRenderComponent = AddComponent<UTextRenderComponent>();
	//TextRenderComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 1.3f));
	//TextRenderComponent->SetText("UUID : " + TextRenderComponent->GetOwnerUUIDToString());
	//TextRenderComponent->AttachToComponent(StaticMeshComponent);
	//TextRenderComponent->SetFont(FName("Default"));

	// SubUV 파티클
	//SubUVComponent = AddComponent<USubUVComponent>();
	//SubUVComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 2.0f));
	//SubUVComponent->SetParticle(FName("Explosion"));
	//SubUVComponent->AttachToComponent(StaticMeshComponent);
	//SubUVComponent->SetVisibility(true);
}

bool AStaticMeshActor::IsBasicShapeAssetPath(const FString& Path) {
	FString NormalizedPath = Path;
	for (char& Character : NormalizedPath)
	{
		if (Character == '\\')
		{
			Character = '/';
		}
	}

	if (NormalizedPath.find("Asset/Content/BasicShape/") != FString::npos ||
		NormalizedPath.find("Asset/Source/BasicShape/") != FString::npos ||
		NormalizedPath.find("Asset/Engine/Content/BasicShape/") != FString::npos ||
		NormalizedPath.find("Asset/Engine/Source/BasicShape/") != FString::npos ||
		NormalizedPath.find("/BasicShape/") != FString::npos)
	{
		return true;
	}

	const char* BasicShapeMeshKeys[] = {
				"Default.Mesh.BasicShape.Cone",
				"Default.Mesh.BasicShape.Cube",
				"Default.Mesh.BasicShape.Cylinder",
				"Default.Mesh.BasicShape.Plane",
				"Default.Mesh.BasicShape.Sphere",
				"Default.Mesh.BasicShape.SphereLowpoly"
	};

	for (const char* MeshKey : BasicShapeMeshKeys)
	{
		if (const FMeshResource* MeshResource = FResourceManager::Get().FindMesh(FName(MeshKey)))
		{
			if (MeshResource->Path == Path)
			{
				return true;
			}
		}
	}

	return false;
}
