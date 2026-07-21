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
 * 이 베이스는 "언제 패링 윈도우를 열지"를 결정하지 않고, 열기/닫기/성공 처리만 제공한다.
 * 파생 GA(GA_Parry, GA_Guard)가 시도 몽타주의 AnimNotifyState_ParryWindow Begin/End에서
 * OpenNotifyParryWindow/CloseNotifyParryWindow 훅으로 윈도우를 프레임 정확하게 여닫는다.
 */
UCLASS(Abstract)
class RETRIEVE_API UGA_ParryBase : public URetrieveGameplayAbility
{
	GENERATED_BODY()

public:
	AActor* GetLastParriedAttacker() const { return LastParriedAttacker.Get(); }

protected:
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	
	// 패링 윈도우(State.Player.Parrying)만 연다. 연타 억제는 쿨다운이 아니라 시도 몽타주 후딜(재발동 불가)이 담당한다.
	bool OpenParryWindow();

	// 패링 윈도우 GE를 명시적으로 제거한다. 성공, NotifyState End, Ability 종료에서 중복 호출되어도 안전해야 한다.
	void CloseParryWindow();

	void StartListeningForParrySuccess();
	void StopParrySuccessTask();
	void ExecuteParrySuccessCue() const;

	// 장착 무기의 패링 데이터(FWeaponParryData). 미장착/데이터 없음이면 nullptr.
	const struct FWeaponParryData* ResolveParryData() const;
	// 무기가 패링 가능한지(= Parry.SuccessMontage 존재).
	bool WeaponCanParry() const;
	void PlayParrySuccessMontage() const;

	UFUNCTION()
	void HandleParrySuccess(FGameplayEventData Payload);

protected:
	// State.Player.Parrying을 부여하는 윈도우 GE. 무기 무관 공용이라 GA에 둔다(DA 데이터 아님).
	UPROPERTY(EditDefaultsOnly, Category = "Parry")
	TSubclassOf<UGameplayEffect> ParryWindowEffect;


	/** 패링 성공 시 짧게 부여하는 공격 어드밴티지 버프 (GE_ParryMomentum).
	 *  미지정이면 C++ 기본 경로(/Game/Retrieve/AbilitySystem/Player/Advantage/GE_ParryMomentum)를 폴백 로드한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Parry|Advantage")
	TSubclassOf<UGameplayEffect> ParryMomentumEffect;

	/** 패링 성공 시 충전할 원소 게이지량. 0 = 비활성 */
	UPROPERTY(EditDefaultsOnly, Category = "Parry|Advantage", meta = (ClampMin = "0"))
	int32 ParryElementGaugeCharge = 15;

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

};
