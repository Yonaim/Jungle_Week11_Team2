#include "PCH/LunaticPCH.h"
#include "PawnActor.h"

#include "Component/BillboardComponent.h"
#include "Component/CameraComponent.h"
#include "Component/SceneComponent.h"
#include "Engine/Runtime/Engine.h"
#include "Resource/ResourceManager.h"
#include "Texture/Texture2D.h"

IMPLEMENT_CLASS(APawnActor, AActor)

APawnActor::APawnActor()
{
	bNeedsTick = true;
	bTickInEditor = true;
}

bool APawnActor::ShouldAutoPossessPlayer(int32 PlayerIndex) const
{
    return PlayerIndex == 0 && AutoPossessPlayer == EAutoPossessPlayer::Player0;
}

void APawnActor::BeginPlay()
{
	Super::BeginPlay();
	// GameMode가 런타임에 스폰한 Pawn은 InitDefaultComponents가 안 돈 상태로 들어올 수 있다.
	// 최소 루트 SceneComponent와 게임용 CameraComponent를 보장해서, Pawn 하나만 배치해도 PIE에서 Possess 대상이 된다.
	if (!GetRootComponent())
	{
		USceneComponent* Root = AddComponent<USceneComponent>();
		Root->SetCanDeleteFromDetails(false);
		SetRootComponent(Root);
	}

	GetOrCreateGameplayCameraComponent();
}

UCameraComponent* APawnActor::GetOrCreateGameplayCameraComponent()
{
	if (GameplayCameraComponent && IsAliveObject(GameplayCameraComponent))
	{
		return GameplayCameraComponent;
	}

	for (UActorComponent* Component : GetComponents())
	{
		if (UCameraComponent* Camera = Cast<UCameraComponent>(Component))
		{
			GameplayCameraComponent = Camera;
			return GameplayCameraComponent;
		}
	}

	GameplayCameraComponent = AddComponent<UCameraComponent>();
	GameplayCameraComponent->SetCanDeleteFromDetails(false);
	if (USceneComponent* Root = GetRootComponent())
	{
		GameplayCameraComponent->AttachToComponent(Root);
	}
	else
	{
		SetRootComponent(GameplayCameraComponent);
	}

	// 기본 Pawn 카메라는 Pawn 위치에서 약간 위쪽을 보도록 둔다.
	// 별도 CameraComponent가 이미 있으면 그 설정을 우선 사용한다.
	GameplayCameraComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 1.6f));
	GameplayCameraComponent->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
	return GameplayCameraComponent;
}

void APawnActor::InitDefaultComponents()
{
	RootSceneComponent = AddComponent<USceneComponent>();
	RootSceneComponent->SetCanDeleteFromDetails(false);
	SetRootComponent(RootSceneComponent);

	GetOrCreateGameplayCameraComponent();

	BillboardComponent = AddComponent<UBillboardComponent>();
	BillboardComponent->SetCanDeleteFromDetails(false);
	BillboardComponent->AttachToComponent(RootSceneComponent);
	BillboardComponent->SetAbsoluteScale(true);
	BillboardComponent->SetEditorOnlyComponent(true);
	BillboardComponent->SetHiddenInComponentTree(true);
	BillboardComponent->SetSpriteSize(1.0f, 1.0f);

	const FString IconPath = FResourceManager::Get().ResolvePath(FName("Editor.Icon.Pawn"));
	ID3D11Device* Device = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;
	if (UTexture2D* Texture = UTexture2D::LoadFromFile(IconPath, Device))
	{
		BillboardComponent->SetTexture(Texture);
	}
}
