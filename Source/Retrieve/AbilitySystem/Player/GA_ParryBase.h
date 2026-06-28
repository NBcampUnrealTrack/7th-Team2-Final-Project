#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "ActiveGameplayEffectHandle.h"
#include "GA_ParryBase.generated.h"

class UAbilityTask_WaitGameplayEvent;
class UGameplayEffect;

/**
 * 패링 공용 베이스(추상).
 *
 * 이 베이스는 "언제 패링 윈도우를 열지"를 결정하지 않고, 열기/닫기/쿨다운/성공 처리만 제공한다.
 * - GA_Parry: 기존 비방패 패링. Activate 시 즉시 Open, 타이머/EndAbility로 Close.
 * - GA_GuardAttack: GuardAttack 몽타주의 NotifyState Begin/End로 Open/Close.
 *
 * 이 분리를 유지해야 기존 패링 감각을 보존하면서, 검방패 GuardAttack만 프레임 정확한 NotifyState 구조로 이전할 수 있다.
 */
UCLASS(Abstract)
class RETRIEVE_API UGA_ParryBase : public URetrieveGameplayAbility
{
	GENERATED_BODY()

public:
	AActor* GetLastParriedAttacker() const { return LastParriedAttacker.Get(); }

protected:
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	
	// 패링 윈도우만 연다. 쿨다운은 여기서 적용하지 않는다.
	// 성공/실패/종료 시점에서 ApplyParryCooldown()을 별도로 호출해 window timing과 cooldown timing을 분리한다.
	bool OpenParryWindow();

	// 패링 윈도우 GE를 명시적으로 제거한다. 성공, NotifyState End, Ability 종료에서 중복 호출되어도 안전해야 한다.
	void CloseParryWindow();

	// 다음 패링 시도를 제한하는 쿨다운만 적용한다. Guard 유지나 CounterWindow는 막지 않는 것이 설계 의도다.
	void ApplyParryCooldown();
	
	void StartListeningForParrySuccess();
	void StopParrySuccessTask();
	void ExecuteParrySuccessCue() const;
	void ApplyParryStagger(AActor* Attacker);

	UFUNCTION()
	void HandleParrySuccess(FGameplayEventData Payload);

protected:
	// Legacy GA_Parry는 duration GE를 사용할 수 있고, GuardAttack은 Infinite/Controlled GE를 사용한다.
	// 둘 다 이 포인터로 주입하되, 실제 길이는 각 Ability의 Open/Close 타이밍 정책이 결정한다.
	UPROPERTY(EditDefaultsOnly, Category = "Parry")
	TSubclassOf<UGameplayEffect> ParryWindowEffect;

	// 플레이어 본인에게 적용되는 "다음 패링 시도 제한" 효과다. 공격자/몬스터에게 적용되는 효과가 아니다.
	UPROPERTY(EditDefaultsOnly, Category = "Parry")
	TSubclassOf<UGameplayEffect> ParryCooldownEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Parry")
	TSubclassOf<UGameplayEffect> CounterWindowEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Parry")
	TSubclassOf<UGameplayEffect> ParryStaggerEffect;
	
	UPROPERTY(EditDefaultsOnly, Category = "Parry")
	TSubclassOf<UGameplayEffect> BossParryStaggerEffect;
	
	UPROPERTY(EditDefaultsOnly, Category = "Parry")
	TSubclassOf<UGameplayEffect> StaminaRestoreEffect;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ParrySuccessTask;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> LastParriedAttacker;

	// NotifyState 기반 window는 GE duration에 기대지 않고 이 handle로 직접 닫는다.
	UPROPERTY(Transient)
	FActiveGameplayEffectHandle ParryWindowHandle;
	
	// GE가 열렸다는 로컬 상태. 중복 Open을 막고, EndAbility에서 실패/만료 cooldown 적용 여부를 판단한다.
	UPROPERTY(Transient)
	bool bParryWindowOpened = false;

	// 성공 경로와 실패/만료 경로를 구분하기 위한 상태. 현재는 기록 목적이며, 후속 정책 분리에 대비한다.
	UPROPERTY(Transient)
	bool bParrySucceeded = false;

	// 성공 처리와 EndAbility cleanup이 모두 호출될 수 있으므로 쿨다운 중복 적용을 막는다.
	UPROPERTY(Transient)
	bool bParryCooldownApplied = false;
};
