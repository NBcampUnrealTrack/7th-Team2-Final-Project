#include "AbilitySystem/Enemy/GAS/GA_Boss_PhaseTransition.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "GameplayTags/RetrieveGameplayTags.h"

UGA_Boss_PhaseTransition::UGA_Boss_PhaseTransition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilityTags.AddTag(RetrieveGameplayTags::Ability_Boss_PhaseTransition);
	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Boss_PhaseTransition);
	
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Dead);

	CancelAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Boss_PatternAttack);
	CancelAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Enemy_Attack);
	CancelAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Enemy_SpecialAttack);

	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Boss_PatternAttack);
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Enemy_Attack);
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Enemy_SpecialAttack);

	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = RetrieveGameplayTags::GameplayEvent_Boss_PhaseTransition;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);
}

void UGA_Boss_PhaseTransition::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	CancelInterruptedAbilities();

	UAnimMontage* Montage = ResolveMontage(TriggerEventData);
	if (!Montage)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, Montage, 1.f, NAME_None, true);

	if (!MontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageInterrupted);
	MontageTask->ReadyForActivation();
}

void UGA_Boss_PhaseTransition::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (!IsEndAbilityValid(Handle, ActorInfo))
	{
		return;
	}

	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_Boss_PhaseTransition::CancelAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateCancelAbility)
{
	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}

void UGA_Boss_PhaseTransition::CancelInterruptedAbilities()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return;
	}

	FGameplayTagContainer TagsToCancel;
	TagsToCancel.AddTag(RetrieveGameplayTags::Ability_Boss_PatternAttack);
	TagsToCancel.AddTag(RetrieveGameplayTags::Ability_Enemy_Attack);
	TagsToCancel.AddTag(RetrieveGameplayTags::Ability_Enemy_SpecialAttack);

	ASC->CancelAbilities(&TagsToCancel, nullptr, this);
}

UAnimMontage* UGA_Boss_PhaseTransition::ResolveMontage(const FGameplayEventData* TriggerEventData) const
{
	if (!TriggerEventData)
	{
		return nullptr;
	}

	const UObject* OptionalObject = TriggerEventData->OptionalObject.Get();
	return const_cast<UAnimMontage*>(Cast<UAnimMontage>(OptionalObject));
}

void UGA_Boss_PhaseTransition::FinishAbility(bool bWasCancelled)
{
	if (!IsActive())
	{
		return;
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
}

void UGA_Boss_PhaseTransition::OnMontageCompleted()
{
	FinishAbility(false);
}

void UGA_Boss_PhaseTransition::OnMontageInterrupted()
{
	FinishAbility(true);
}
