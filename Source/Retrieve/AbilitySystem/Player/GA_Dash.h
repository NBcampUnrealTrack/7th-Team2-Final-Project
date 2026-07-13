#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "GA_Dash.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;
class UGameplayEffect;
/**
 * 
 */
UCLASS()
class RETRIEVE_API UGA_Dash : public URetrieveGameplayAbility
{
	GENERATED_BODY()
	
public:
	UGA_Dash();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	// 대시 몽타주의 AnimNotifyState_IFrameWindow가 요청하는 무적 윈도우 hook.
	// Open은 IFrameWindowEffect(State.Player.Invincible 부여)를 self에 적용, Close는 제거한다.
	virtual bool OpenNotifyIFrameWindow() override;
	virtual void CloseNotifyIFrameWindow() override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	FVector ResolveDashDirection(const FGameplayAbilityActorInfo* ActorInfo) const;

	UFUNCTION() void HandleMontageFinished();

private:
	UPROPERTY(EditDefaultsOnly, Category = "Dash")
	TSoftObjectPtr<UAnimMontage> DashMontage;

	/** Dash 몽타주 기본 재생 속도. 1.0=ALS 원본. 최종 PlayRate = Base × (MoveSpeed / ReferenceMoveSpeed). */
	UPROPERTY(EditDefaultsOnly, Category = "Dash", meta = (ClampMin = "0.1"))
	float BaseDashPlayRate = 1.35f;

	/** i-frame 무적 GE. Infinite + GrantedTag=State.Player.Invincible 로 설정. ANS 윈도우 구간 동안만 적용된다. */
	UPROPERTY(EditDefaultsOnly, Category = "Dash|IFrame")
	TSubclassOf<UGameplayEffect> IFrameWindowEffect;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	// 무적 GE 핸들/중복 Open 가드. ANS Begin/End와 EndAbility가 idempotent하게 열고 닫는다.
	FActiveGameplayEffectHandle IFrameWindowHandle;
	bool bIFrameWindowOpened = false;
};
