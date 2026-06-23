#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "GA_EquipTransition.generated.h"

class UAbilityTask_PlayMontageAndWait;

/**
 * 장착(Equip) / 해제(Unequip) 전환 어빌리티. (무기 교체는 Equip로 흡수)
 * WeaponComponent가 Equip/Unequip 시 GameplayEvent(Equip/Unequip)를 보내면 트리거된다.
 *
 * - 몽타주는 현재 링크된 무기 레이어(URetrieveAlsLinkedAnimInstance)의 Equip/UnequipMontage에서 가져온다.
 * - 메시 스폰/파괴는 몽타주 내 AnimNotify_SetWeaponVisuals가 담당한다.
 * - 정상 완료면 노티가 비주얼을 맞춰 끝나므로 그대로 종료하고,
 *   인터럽트/취소/몽타주 없음일 때만 WeaponComponent::ReconcileVisuals로 데이터에 맞춰 봉인한다.
 * - GA_EquipTransition은 무기 AbilitySet이 아니라 캐릭터 상시 AbilitySet에 grant해야 한다
 *   (Unequip 시 ClearWeaponData가 무기 부여분을 먼저 회수하므로).
 * - LocalPredicted라 몽타주 재생/복제는 GAS가 처리.
 */
UCLASS()
class RETRIEVE_API UGA_EquipTransition : public URetrieveGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_EquipTransition();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	// 정상 완료: 노티가 비주얼을 맞췄으니 reconcile 없이 종료.
	UFUNCTION() 
	void HandleMontageCompleted();
	
	// 인터럽트/취소: 노티가 못 돌았을 수 있으니 reconcile 경로로 종료.
	UFUNCTION() 
	void HandleMontageInterrupted();

	void ReconcileWeaponVisuals();

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;
};