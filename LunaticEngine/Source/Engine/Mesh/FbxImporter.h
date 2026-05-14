#pragma once

#include "Core/CoreTypes.h"

struct FStaticMaterial;
struct FStaticMesh;
struct FSkeletalMesh;

struct FFbxStaticMeshImporter
{
	static bool Import(const FString& FbxFilePath, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials);
};

struct FFbxSkeletalMeshImporter
{
	static bool Import(const FString& FbxFilePath, FSkeletalMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials);
};
