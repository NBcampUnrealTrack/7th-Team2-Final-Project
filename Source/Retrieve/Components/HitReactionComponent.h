#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"
#include "Combat/RetrieveCombatTypes.h"
#include "HitReactionComponent.generated.h"

class UAnimMontage;
class UGameplayEffect;
class URetrieveAbilitySystemComponent;
class URetrieveHitReactionProfile;
struct FGameplayEventData;

/**
 * 플레이어 전용 피격 반응 consumer
 *  - Flinch  : 짧은 cosmetic 몽타주만 (태그/GE/취소 없음)
 *  - Stagger : GE_HitStagger(State.Player.Staggered) + 진행 중 능력 취소 + 몽타주
 *  - Knockdown: GE_HitKnockdown(State.Player.Knockdown) + 취소 + 몽타주
 */
UCLASS(ClassGroup = "Retrieve", meta = (BlueprintSpawnableComponent))
class RETRIEVE_API UHitReactionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHitReactionComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	void Configure(URetrieveHitReactionProfile* InProfile);

private:
	URetrieveAbilitySystemComponent* GetASC() const;
	
	void OnAbilitySystemReady();
	
	void HandleHitEvent(FGameplayTag MatchingTag, const FGameplayEventData* Payload);

	static ERetrieveHitReactType ResolveReactType(const FGameplayEventData* Payload);

	void ApplyReaction(ERetrieveHitReactType ReactType);
	void ApplyStateEffect(const TSubclassOf<UGameplayEffect>& EffectClass);
	void CancelPlayerActions();
	void PlayMontageSafe(const TSoftObjectPtr<UAnimMontage>& MontagePtr);
	void StopActiveMontage();

private:
	UPROPERTY(Transient) 
	TObjectPtr<URetrieveHitReactionProfile> Profile;

	UPROPERTY(Transient) 
	TObjectPtr<UAnimMontage> ActiveMontage;

	FDelegateHandle HitEventHandle;
	
	bool bProcessingReaction = false;
};
