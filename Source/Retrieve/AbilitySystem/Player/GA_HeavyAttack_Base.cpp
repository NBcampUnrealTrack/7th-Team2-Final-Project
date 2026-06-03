#include "AbilitySystem/Player/GA_HeavyAttack_Base.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Components/ElementGaugeComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"

UGA_HeavyAttack_Base::UGA_HeavyAttack_Base()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	
	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_HeavyAttack);
	SetAssetTags(Tags);
	
	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Player_UsingHeavyAttack);
	
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dodging);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Guarding);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Knockdown);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dead);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_UsingHeavyAttack);
	
	CancelAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Attack);
	
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Attack);
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Guard);
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
	
	const FGameplayTag ConsumedElement = Gauge->ConsumeOldestSlot();

	if (!ConsumedElement.IsValid() || ConsumedElement == RetrieveGameplayTags::Element_None)
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
