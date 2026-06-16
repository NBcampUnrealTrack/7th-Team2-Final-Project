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
 * 범용 피격 반응 consumer (플레이어/적 공용)
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
	void CancelOwnerAbilities();
	void PlayMontageSafe(const TSoftObjectPtr<UAnimMontage>& MontagePtr);
	void StopActiveMontage();

private:
	// 이 태그들 중 하나라도 오너 ASC에 있으면 피격 반응을 건너뛴다(데미지는 정상 적용).
	// 예: 버스트 시전 중 슈퍼아머. 플레이어/적 공용이므로 인스턴스별로 조정 가능.
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|HitReaction")
	FGameplayTagContainer ReactionSuppressionTags;

	UPROPERTY(Transient)
	TObjectPtr<URetrieveHitReactionProfile> Profile;

	UPROPERTY(Transient) 
	TObjectPtr<UAnimMontage> ActiveMontage;

	FDelegateHandle HitEventHandle;
	
	bool bProcessingReaction = false;
};
