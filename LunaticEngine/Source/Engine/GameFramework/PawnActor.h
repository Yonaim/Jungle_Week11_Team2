#pragma once

#include "GameFramework/AActor.h"

class USceneComponent;
class UBillboardComponent;
class UCameraComponent;

enum class EAutoPossessPlayer : uint8
{
    Disabled = 0,
    Player0 = 1
};

class APawnActor : public AActor
{
public:
	DECLARE_CLASS(APawnActor, AActor)

	APawnActor();

	void BeginPlay() override;

	void InitDefaultComponents();

	EAutoPossessPlayer GetAutoPossessPlayer() const { return AutoPossessPlayer; }
	void SetAutoPossessPlayer(EAutoPossessPlayer InAutoPossessPlayer) { AutoPossessPlayer = InAutoPossessPlayer; }
	bool ShouldAutoPossessPlayer(int32 PlayerIndex) const;

	// PIE/Game에서 배치된 Pawn만 있어도 조종 가능한 기본 카메라를 보장한다.
	UCameraComponent* GetOrCreateGameplayCameraComponent();
	UCameraComponent* GetGameplayCameraComponent() const { return GameplayCameraComponent; }

private:
    // Unreal의 Auto Possess Player 개념을 단순화한 설정.
    // 현재 엔진은 로컬 Player0만 지원하므로 기본값을 Player0로 둔다.
    EAutoPossessPlayer AutoPossessPlayer = EAutoPossessPlayer::Player0;

	USceneComponent* RootSceneComponent = nullptr;
	UBillboardComponent* BillboardComponent = nullptr;
	UCameraComponent* GameplayCameraComponent = nullptr;
};
