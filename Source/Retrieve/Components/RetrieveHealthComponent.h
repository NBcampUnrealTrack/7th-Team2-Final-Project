#pragma once

#include "CoreMinimal.h"
#include "Components/PawnComponent.h"
#include "GameplayEffectTypes.h"
#include "RetrieveHealthComponent.generated.h"

class UCombatAttributeSet;
class URetrieveAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHealthChanged, float, NewHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeathStarted, AActor*, OwningActor);

/**
 * HealthComponent가 이벤트를 소유하고, AttributeSet이 값을 소유합니다.
 * 체력이 0 이하로 떨어지면 OnDeathStarted를 발동합니다.
 * 아키타입별 사망 처리 로직은 OnDeathStarted를 구독하는 캐릭터 서브클래스에서 구현합니다.
 */
UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent))
class RETRIEVE_API URetrieveHealthComponent : public UPawnComponent
{
	GENERATED_BODY()

public:
	URetrieveHealthComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	void InitializeWithAbilitySystem(URetrieveAbilitySystemComponent* InASC);
	void UninitializeWithAbilitySystem();
	
	UFUNCTION(BlueprintPure, Category = "Retrieve|Health")
	float GetHealth() const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Health")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Health")
	bool IsDeadOrDying() const { return bDeathStarted; }
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	UPROPERTY(BlueprintAssignable)
	FOnHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable)
	FOnHealthChanged OnMaxHealthChanged;

	UPROPERTY(BlueprintAssignable)
	FOnDeathStarted OnDeathStarted;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void HandleHealthChanged(const FOnAttributeChangeData& ChangeData);
	virtual void HandleMaxHealthChanged(const FOnAttributeChangeData& ChangeData);

	UFUNCTION()
	void OnRep_DeathStarted();

	UPROPERTY()
	TObjectPtr<URetrieveAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<const UCombatAttributeSet> AttributeSet;

	UPROPERTY(ReplicatedUsing = OnRep_DeathStarted)
	bool bDeathStarted = false;
};
