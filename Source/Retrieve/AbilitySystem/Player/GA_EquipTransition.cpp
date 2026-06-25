#include "GA_EquipTransition.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Character/Cosmetics/RetrieveAlsLinkedAnimInstance.h"
#include "Components/Player/WeaponComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameplayTags/RetrieveGameplayTags.h"

UGA_EquipTransition::UGA_EquipTransition()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_EquipTransition);
	SetAssetTags(Tags);

	// WeaponComponent가 SendGameplayEvent(Equip/Unequip)하면 발동되는 이벤트 트리거.
	FAbilityTriggerData EquipTrigger;
	EquipTrigger.TriggerTag = RetrieveGameplayTags::GameplayEvent_Player_EquipWeapon;
	EquipTrigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(EquipTrigger);

	FAbilityTriggerData UnequipTrigger;
	UnequipTrigger.TriggerTag = RetrieveGameplayTags::GameplayEvent_Player_UnequipWeapon;
	UnequipTrigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(UnequipTrigger);

	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dead);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Knockdown);
}

void UGA_EquipTransition::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	const bool bEquip = TriggerEventData
		&& TriggerEventData->EventTag == RetrieveGameplayTags::GameplayEvent_Player_EquipWeapon;

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true); // 비주얼 봉인
		return;
	}

	// 현재 링크된 무기 레이어에서 장착/해제 몽타주를 가져온다(무기별 데이터는 레이어가 소유).
	UAnimMontage* Montage = nullptr;
	if (const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		if (const USkeletalMeshComponent* Mesh = Character->GetMesh())
		{
			for (UAnimInstance* Linked : Mesh->GetLinkedAnimInstances())
			{
				if (URetrieveAlsLinkedAnimInstance* Layer = Cast<URetrieveAlsLinkedAnimInstance>(Linked))
				{
					Montage = bEquip ? Layer->EquipMontage : Layer->UnequipMontage;
					break;
				}
			}
		}
	}

	// 몽타주가 없으면(레이어 미설정 등) 노티가 못 도므로, reconcile 경로로 종료해 데이터에 맞춰둔다.
	if (!IsValid(Montage))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage);
	if (!MontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 정상 완료(Completed/BlendOut) → reconcile 없이 종료. 인터럽트/취소 → reconcile.
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageInterrupted);

	// Equip 전환 게이트 태그: WeaponComponent가 스폰을 숨기고(hidden), CombatStance가 자동부착을 노티에 위임한다.
	// (몽타주가 확정된 경우에만 부여 — 몽타주 없는 fallback은 즉시 visible 스폰이라 켜지 않는다)
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->AddLooseGameplayTag(RetrieveGameplayTags::Ability_Player_EquipTransition);
	}

	MontageTask->ReadyForActivation();
}

void UGA_EquipTransition::HandleMontageCompleted()
{
	// 몽타주가 끝까지 갔으니 교체 OLD 메시를 이제 파괴(NEW는 발검 노티로 이미 손에 안착).
	if (const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UWeaponComponent* Weapon = Character->FindComponentByClass<UWeaponComponent>())
		{
			Weapon->DestroyPendingVisuals();
			Weapon->FinalizeEquipTransitionVisuals(); // 노티가 블렌드로 누락돼도 최종 비주얼 보장
		}
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_EquipTransition::HandleMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_EquipTransition::ReconcileWeaponVisuals()
{
	if (const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UWeaponComponent* Weapon = Character->FindComponentByClass<UWeaponComponent>())
		{
			Weapon->DestroyPendingVisuals(); // 캔슬/인터럽트: 보류된 OLD도 파괴하고 데이터로 리빌드
			Weapon->ReconcileVisuals();
		}
	}
}

void UGA_EquipTransition::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	// 게이트 태그 해제(부여 안 됐으면 count 0 → 무해). reconcile/이후 스폰이 정상 경로(visible)를 타도록 먼저 푼다.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->RemoveLooseGameplayTag(RetrieveGameplayTags::Ability_Player_EquipTransition);
	}

	// bWasCancelled = '노티가 비주얼을 못 맞췄을 수 있음'(인터럽트/취소/몽타주 없음/커밋 실패)일 때만 데이터로 봉인.
	// 정상 완료는 노티가 이미 처리했으므로 재생성하지 않는다.
	if (bWasCancelled)
	{
		ReconcileWeaponVisuals();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}