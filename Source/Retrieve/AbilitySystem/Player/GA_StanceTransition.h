#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "GA_StanceTransition.generated.h"

class UAbilityTask_PlayMontageAndWait;

/**
 * 발검(Draw) / 납검(Sheathe) 전환 어빌리티.
 * CombatStanceComponent가 납검 경계를 넘을 때 GameplayEvent(Draw/Sheathe)를 보내면 트리거된다.
 *
 * - 어떤 몽타주를 쓸지는 현재 링크된 무기 레이어(URetrieveAlsLinkedAnimInstance)의 Draw/Sheathe 몽타주에서 가져온다.
 * - 납검(Sheathe) 중에만 State.Player.StanceBusy를 부여해 공격을 잠근다(발검은 즉시 전투 진입을 위해 안 잠금).
 * - LocalPredicted라 몽타주 재생/복제는 GAS가 처리(직접 RPC 불필요).
 */
UCLASS()
class RETRIEVE_API UGA_StanceTransition : public URetrieveGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_StanceTransition();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	// 정상 완료: 노티가 소켓을 맞췄으니 봉인 없이 종료.
	UFUNCTION() 
	void HandleMontageCompleted();
	
	// 인터럽트/취소: 노티가 못 돌았을 수 있으니 SetWeaponDrawn으로 소켓을 봉인하고 종료.
	UFUNCTION() 
	void HandleMontageInterrupted();

	void SealWeaponSocket();

	// 이번 전환이 발검(true)/납검(false)인지. 캔슬/무몽타주 종료 시 SetWeaponDrawn(bDraw) 봉인에 사용.
	bool bDraw = false;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;
};