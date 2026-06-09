#include "AbilitySystem/Player/GA_HeavyAttack_Base.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Components/ElementGaugeComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"

UGA_HeavyAttack_Base::UGA_HeavyAttack_Base()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_HeavyAttack);
	SetAssetTags(Tags);

	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Player_UsingHeavyAttack);

	// 공중/점프 중 강공격 불가
	bBlockActivationWhileAirborne = true;

	// 상태 게이트(회피/경직/다운/사망) 중 + 자기 재발동 차단
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dodging);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Knockdown);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dead);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_UsingHeavyAttack);

	CancelAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Attack);

	// 시전 중 다른 액션 차단 (공격/가드/대시/버스트 모두 발동 불가)
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Attack);
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Guard);
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Dash);
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Burst);
}

void UGA_HeavyAttack_Base::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UElementGaugeComponent* Gauge = IsValid(AvatarActor) ? AvatarActor->FindComponentByClass<UElementGaugeComponent>() : nullptr;
	if (!IsValid(Gauge))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	if (!Gauge->HasChargedSlot())
	{
		ExecuteOwnerCue(RetrieveGameplayTags::GameplayCue_HeavyAttack_NoSlot);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FGameplayTag ConsumedElement = Gauge->ConsumeOldestSlot();
	if (!ConsumedElement.IsValid() || ConsumedElement == RetrieveGameplayTags::Element_None)
	{
		ExecuteOwnerCue(RetrieveGameplayTags::GameplayCue_HeavyAttack_NoSlot);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	ExecuteHeavyEffect(ConsumedElement);
}

void UGA_HeavyAttack_Base::PlayHeavyMontageThenEnd()
{
	UAnimMontage* Montage = HeavyMontage.LoadSynchronous();
	if (!IsValid(Montage))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
		return;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, Montage, MontagePlayRate, NAME_None, /*bStopWhenAbilityEnds=*/true);
	if (!MontageTask)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageFinished);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageFinished);
	MontageTask->ReadyForActivation();
}

void UGA_HeavyAttack_Base::HandleMontageFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}

void UGA_HeavyAttack_Base::ExecuteOwnerCue(const FGameplayTag& CueTag) const
{
	if (!CueTag.IsValid())
	{
		return;
	}
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		FGameplayCueParameters Params;
		Params.Instigator = GetAvatarActorFromActorInfo();
		ASC->ExecuteGameplayCue(CueTag, Params);
	}
}

void UGA_HeavyAttack_Base::ExecuteHeavyEffect(const FGameplayTag& /*ConsumedElement*/)
{
	ensureMsgf(false, TEXT("UGA_HeavyAttack_Base::ExecuteHeavyEffect 미구현: %s. 자식 클래스에서 override 필요."), *GetName());
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
}

void UGA_HeavyAttack_Base::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
