#pragma once

#include "GameFramework/AActor.h"

class USkeletalMeshComponent;

class ASkeletalMeshActor : public AActor
{
public:
	DECLARE_CLASS(ASkeletalMeshActor, AActor)
	ASkeletalMeshActor() {}

	virtual void InitDefaultComponents(const FString& FbxFileName = "None");

	USkeletalMeshComponent* GetSkeletalMeshComponent() const { return SkeletalMeshComponent; }

protected:
	USkeletalMeshComponent* SkeletalMeshComponent = nullptr;
};
