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
	// 피격 이벤트의 Instigator(공격자)가 보스인지. AttackStateSuppressionTags 슈퍼아머의 보스 예외 판정용.
	bool IsInstigatorBoss(const FGameplayEventData* Payload) const;
	void PlayMontageSafe(const TSoftObjectPtr<UAnimMontage>& MontagePtr);
	void StopActiveMontage();

private:
	// 이 태그들 중 하나라도 오너 ASC에 있으면 피격 반응을 건너뛴다(데미지는 정상 적용).
	// 예: 버스트 시전 중 슈퍼아머. 모든 공격자에 대해 무조건 억제. 플레이어/적 공용이므로 인스턴스별로 조정 가능.
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|HitReaction")
	FGameplayTagContainer ReactionSuppressionTags;

	// 오너가 이 태그 보유(=공격 중)면 "보스가 아닌" 공격자의 피격 반응만 건너뛴다(부분 슈퍼아머, 데미지는 적용).
	// 기본값=플레이어 공격 태그라 적에는 영향 없음.
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|HitReaction")
	FGameplayTagContainer AttackStateSuppressionTags;

	// AttackStateSuppressionTags 억제 중에도 보스 공격은 예외로 리액션을 허용할지.
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|HitReaction")
	bool bBossBypassesAttackSuppression = true;

	UPROPERTY(Transient)
	TObjectPtr<URetrieveHitReactionProfile> Profile;

	UPROPERTY(Transient) 
	TObjectPtr<UAnimMontage> ActiveMontage;

	FDelegateHandle HitEventHandle;
	
	bool bProcessingReaction = false;
};
