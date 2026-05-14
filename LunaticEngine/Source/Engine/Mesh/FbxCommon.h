#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include "Math/Matrix.h"
#include "FBXSDK/include/fbxsdk.h"

#include <filesystem>
#include <unordered_map>

struct FStaticMaterial;
struct FStaticMesh;
struct FSkeletalMesh;

struct FFbxMaterialInfo
{
	FbxSurfaceMaterial* SourceMaterial = nullptr;
	FString MaterialSlotName = "None";
	FString DiffuseTexturePath;
	FString NormalTexturePath;
	FVector DiffuseColor = FVector(1.0f, 1.0f, 1.0f);
};

struct FFbxInfo
{
	FbxManager* Manager = nullptr;
	FbxScene* Scene = nullptr;
	TArray<FFbxMaterialInfo> Materials;
	TArray<TArray<uint32>> FacesPerMaterial;
	std::unordered_map<FbxSurfaceMaterial*, int32> MaterialMap;
};

struct FFbxVertexKey
{
	int32 ControlPointIndex = -1;
	int32 PolygonIndex = -1;
	int32 CornerIndex = -1;
	int32 MaterialIndex = 0;

	bool operator==(const FFbxVertexKey& Other) const
	{
		return ControlPointIndex == Other.ControlPointIndex
			&& PolygonIndex == Other.PolygonIndex
			&& CornerIndex == Other.CornerIndex
			&& MaterialIndex == Other.MaterialIndex;
	}
};

struct FFbxVertexKeyHash
{
	size_t operator()(const FFbxVertexKey& Key) const noexcept;
};

struct FFbxCommon
{
	static bool ParseFbx(const FString& FbxFilePath, FFbxInfo& OutContext);
	static void Destroy(FFbxInfo& Context);

	static int32 GetPolygonMaterialIndex(FbxMesh* Mesh, int32 PolygonIndex);
	static int32 FindOrAddMaterial(const FString& FbxFilePath, FbxNode* Node, int32 LocalMaterialIndex, FFbxInfo& Context);
	static FString ConvertMaterialInfoToMaterialAsset(const FString& FbxFilePath, const FFbxMaterialInfo& MaterialInfo);

	static FString SanitizeName(FString Name);
	static FVector GetDiffuseColor(FbxSurfaceMaterial* FbxMaterial);
	static FString FindDiffuseTexturePath(const FString& FbxFilePath, FbxSurfaceMaterial* FbxMaterial);
	static FString FindNormalTexturePath(const FString& FbxFilePath, FbxSurfaceMaterial* FbxMaterial);
	static FString ResolveFbxTexturePath(const FString& FbxFilePath, FbxFileTexture* Texture);
	static FString MakeProjectRelativePath(const std::filesystem::path& Path);

	static bool ReadNormal(FbxMesh* Mesh, int32 ControlPointIndex, int32 PolygonIndex, int32 CornerIndex, FbxVector4& OutNormal);
	static FVector RemapVector(const FbxVector4& Vector);
	static FbxVector4 UnmapVector(const FVector& Vector);
	static FMatrix MakeEngineLinearMatrix(const FbxAMatrix& Matrix);
	static FVector TransformNormalByMatrix(const FbxAMatrix& Matrix, const FbxVector4& Normal);
	static FVector2 RemapUV(const FbxVector2& UV);
};
