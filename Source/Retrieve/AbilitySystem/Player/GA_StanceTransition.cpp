#include "AbilitySystem/Player/GA_StanceTransition.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Character/Cosmetics/RetrieveAlsLinkedAnimInstance.h"
#include "Components/Player/WeaponComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameplayTags/RetrieveGameplayTags.h"

UGA_StanceTransition::UGA_StanceTransition()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// 진행 중 전환을 다음 전환이 캔슬할 수 있게 식별 태그 부여(CombatStanceComponent가 이 태그로 CancelAbilities).
	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_StanceTransition);
	SetAssetTags(Tags);

	// 컴포넌트가 SendGameplayEvent(Draw/Sheathe)하면 발동되는 이벤트 트리거.
	FAbilityTriggerData DrawTrigger;
	DrawTrigger.TriggerTag = RetrieveGameplayTags::GameplayEvent_Player_DrawWeapon;
	DrawTrigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(DrawTrigger);

	FAbilityTriggerData SheatheTrigger;
	SheatheTrigger.TriggerTag = RetrieveGameplayTags::GameplayEvent_Player_SheatheWeapon;
	SheatheTrigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(SheatheTrigger);

	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dead);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Knockdown);
}

void UGA_StanceTransition::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	bDraw = TriggerEventData
		&& TriggerEventData->EventTag == RetrieveGameplayTags::GameplayEvent_Player_DrawWeapon;

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 공격은 잠그지 않는다. 공격이 들어오면 그 몽타주가 이 전환 몽타주를 인터럽트 → OnInterrupted로 자연 종료(캔슬).

	// 현재 링크된 무기 레이어에서 발검/납검 몽타주를 가져온다(무기별 데이터는 레이어가 소유).
	UAnimMontage* Montage = nullptr;
	if (const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		if (const USkeletalMeshComponent* Mesh = Character->GetMesh())
		{
			for (UAnimInstance* Linked : Mesh->GetLinkedAnimInstances())
			{
				if (URetrieveAlsLinkedAnimInstance* Layer = Cast<URetrieveAlsLinkedAnimInstance>(Linked))
				{
					Montage = bDraw ? Layer->DrawMontage : Layer->SheatheMontage;
					break;
				}
			}
		}
	}

	// 몽타주가 없으면(레이어 미설정 등) 노티가 못 도므로, 봉인 경로(bWasCancelled=true)로 종료해
	// SetWeaponDrawn(bDraw)로 소켓을 즉시 맞춘다. (SheatheMontage 없어도 납검이 동작)
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

	// 정상 완료(Completed/BlendOut) → 봉인 없이 종료. 인터럽트/취소 → SetWeaponDrawn 봉인.
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
	MontageTask->ReadyForActivation();
}

void UGA_StanceTransition::HandleMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_StanceTransition::HandleMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_StanceTransition::SealWeaponSocket()
{
	if (const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UWeaponComponent* Weapon = Character->FindComponentByClass<UWeaponComponent>())
		{
			Weapon->SetWeaponDrawn(bDraw);
		}
	}
}

void UGA_StanceTransition::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	// bWasCancelled = '노티가 소켓을 못 맞췄을 수 있음'(인터럽트/취소/무몽타주/커밋 실패)일 때만 소켓 봉인.
	// 정상 완료는 노티가 이미 처리. (SetStance가 CancelAbilities를 먼저 하므로, 교체 케이스에선
	//  이 봉인이 이후 instant SetWeaponDrawn보다 앞에서 끝나 충돌하지 않는다.)
	if (bWasCancelled)
	{
		SealWeaponSocket();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}