#include "PCH/LunaticPCH.h"
#include "GameFramework/SkeletalMeshActor.h"
#include "Object/ObjectFactory.h"
#include "Engine/Runtime/Engine.h"
#include "Component/SkeletalMeshComponent.h"
#include "Mesh/MeshAssetManager.h"

IMPLEMENT_CLASS(ASkeletalMeshActor, AActor)

void ASkeletalMeshActor::InitDefaultComponents(const FString& FbxFileName)
{
	SkeletalMeshComponent = AddComponent<USkeletalMeshComponent>();
	SkeletalMeshComponent->SetCanDeleteFromDetails(false);
	SetRootComponent(SkeletalMeshComponent);

	if (!FbxFileName.empty() && FbxFileName != "None")
	{
		ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
		USkeletalMesh* Asset = FMeshAssetManager::LoadSkeletalMesh(FbxFileName, Device);
		SkeletalMeshComponent->SetSkeletalMesh(Asset);
	}
	else
	{
		SkeletalMeshComponent->SetSkeletalMesh(nullptr);
	}
}
