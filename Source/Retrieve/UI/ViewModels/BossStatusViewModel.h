#pragma once

#include "CoreMinimal.h"
#include "Components/SlateWrapperTypes.h"
#include "MVVMViewModelBase.h"
#include "BossStatusViewModel.generated.h"

class URetrieveHealthComponent;

/**
 * 보스 전투 HUD 구동. 보스 조우 시 BindToBoss, 사망/이탈 시 UnbindFromBoss.
 * ARetrievePlayerController::TryBindBossToHUD 에서 호출합니다.
 */
UCLASS(BlueprintType)
class RETRIEVE_API UBossStatusViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI")
	void BindToBoss(URetrieveHealthComponent* InHealth, FText InBossName);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI")
	void UnbindFromBoss();

	// "HUD 숨기기"가 켜져 있으면 bVisible이 true여도 숨김으로 보고한다(MVVM 가시성 바인딩·C++ 양쪽이 이 값을 사용).
	// 체력/이름 등 다른 필드는 그대로 갱신되므로 보스 체력 추적 기능은 유지된다.
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|UI")
	bool GetIsVisible() const;

	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|UI")
	ESlateVisibility GetSlateVisibility() const;

	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|UI")
	FText GetBossName() const { return BossName; }

	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|UI")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|UI")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|UI")
	float GetHealthFraction() const
	{
		return MaxHealth > 0.f ? FMath::Clamp(CurrentHealth / MaxHealth, 0.f, 1.f) : 0.f;
	}

	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|UI")
	float GetDisplayedHealthFraction() const { return DisplayedHealthFraction; }

	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|UI")
	FText GetHealthText() const;

	UPROPERTY(EditAnywhere, Category = "Retrieve|UI")
	float HealthInterpSpeed = 6.f;

private:
	UFUNCTION()
	void HandleHealthChanged(float NewHealth);

	UFUNCTION()
	void HandleMaxHealthChanged(float NewMaxHealth);

	UFUNCTION()
	void HandleDeathStarted(AActor* OwningActor);

	UPROPERTY()
	TWeakObjectPtr<URetrieveHealthComponent> BoundHealth;

	FTSTicker::FDelegateHandle InterpTicker;

	bool bVisible = false;
	FText BossName;
	float CurrentHealth = 0.f;
	float MaxHealth = 1.f;
	float DisplayedHealthFraction = 0.f;

	bool TickInterp(float DeltaTime);
};
