#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "RetrieveGameplayAbility.generated.h"

class UAbilityTask_WaitGameplayEvent;
class UGameplayEffect;

UENUM(BlueprintType)
enum class ERetrieveAbilityActivationPolicy : uint8
{
	OnInputTriggered,
	OnSpawn,
	WhileInputActive // reserved; not used in MVP
};

UCLASS(Abstract)
class RETRIEVE_API URetrieveGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	URetrieveGameplayAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	ERetrieveAbilityActivationPolicy GetActivationPolicy() const { return ActivationPolicy; }

	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& AbilitySpec) override;
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	
	static bool IsAvatarAirborne(const FGameplayAbilityActorInfo* ActorInfo);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Ability")
	ERetrieveAbilityActivationPolicy ActivationPolicy = ERetrieveAbilityActivationPolicy::OnInputTriggered;
	
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Ability")
	bool bBlockActivationWhileAirborne = false;
	
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Ability|Parry")
	bool bAutoListenForParried = false;
	
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Ability|Parry")
	void StartListeningForParried();
	
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Ability|Parry")
	TSubclassOf<UGameplayEffect> ParriedStaggerEffect;

private:
	void TryActivateAbilityOnSpawn(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& AbilitySpec) const;

	UFUNCTION() void HandleParried(FGameplayEventData Payload);

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ParriedTask;
};
