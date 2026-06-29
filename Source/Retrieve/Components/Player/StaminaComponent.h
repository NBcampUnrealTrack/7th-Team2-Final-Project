#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "Components/ActorComponent.h"
#include "StaminaComponent.generated.h"

class UAbilitySystemComponent;
class UCombatAttributeSet;
class UGameplayEffect;
struct FOnAttributeChangeData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStaminaChanged, float, NewStamina, float, MaxStamina);

/**
 * 스태미너 자원 관리(플레이어, 전 무기 공용)
 */
UCLASS(ClassGroup = (Retrieve), meta = (BlueprintSpawnableComponent))
class RETRIEVE_API UStaminaComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStaminaComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void InitializeWithAbilitySystem(UAbilitySystemComponent* InASC);
	void UninitializeFromAbilitySystem();

	UFUNCTION(BlueprintPure, Category = "Retrieve|Stamina")
	float GetStamina() const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Stamina")
	float GetMaxStamina() const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Stamina")
	bool HasStamina(float Cost) const;
	
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Stamina")
	void ResetStamina();

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Stamina")
	FOnStaminaChanged OnStaminaChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UAbilitySystemComponent* ResolveASC();
	bool TryBindAttributeSet();
	void HandleStaminaAttributeChanged(const FOnAttributeChangeData& Data);
	void HandleMaxStaminaAttributeChanged(const FOnAttributeChangeData& Data);
	void BroadcastStaminaChanged();
	
	UPROPERTY(EditAnywhere, Category = "Retrieve|Stamina")
	TSubclassOf<UGameplayEffect> StaminaRegenEffect;

	// UI(WBP_Stamina) 연동 완료로 비활성화. PIE 온스크린 스태미너 디버그가 필요하면
	// 아래 프로퍼티와 StaminaComponent.cpp TickComponent 내 디버그 블록 주석을 해제.
	//UPROPERTY(EditAnywhere, Category = "Retrieve|Stamina|Debug")
	//bool bShowDebugOnScreen = true;

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(Transient)
	TObjectPtr<const UCombatAttributeSet> AttributeSet;

	FDelegateHandle StaminaChangedHandle;
	FDelegateHandle MaxStaminaChangedHandle;
	
	FActiveGameplayEffectHandle RegenEffectHandle;
};
