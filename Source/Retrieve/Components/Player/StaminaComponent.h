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

	// TODO(하민): PIE 화면에 현재 스태미너 표시. UI 연결 후 제거
	UPROPERTY(EditAnywhere, Category = "Retrieve|Stamina|Debug")
	bool bShowDebugOnScreen = true;

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(Transient)
	TObjectPtr<const UCombatAttributeSet> AttributeSet;

	FDelegateHandle StaminaChangedHandle;
	FDelegateHandle MaxStaminaChangedHandle;
	
	FActiveGameplayEffectHandle RegenEffectHandle;
};
