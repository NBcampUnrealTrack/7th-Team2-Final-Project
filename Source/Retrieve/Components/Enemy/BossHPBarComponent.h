#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BossHPBarComponent.generated.h"

class URetrieveHealthComponent;

/**
 * 보스 HP 바를 HUD에 바인딩/해제하는 컴포넌트.
 * 보스로 사용할 에너미 BP에 추가하고 DisplayName만 설정하면 된다.
 * Show()/Hide() 호출은 스폰너 또는 게임 로직이 담당한다.
 */
UCLASS(ClassGroup=(Retrieve), meta=(BlueprintSpawnableComponent))
class RETRIEVE_API UBossHPBarComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBossHPBarComponent();

	void Show();
	void Hide();

	FText GetDisplayName() const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void TryBindToHUD();
	void ClearFromHUD();

	UPROPERTY(EditAnywhere, Category="Retrieve|Boss HP Bar")
	FText DisplayName;

	FTimerHandle RetryTimer;
	int32 BindAttempts = 0;
};
