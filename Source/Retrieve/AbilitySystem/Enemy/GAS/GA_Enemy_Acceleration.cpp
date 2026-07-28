#include "AbilitySystem/Enemy/GAS/GA_Enemy_Acceleration.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "GameplayEffect.h"
#include "GameplayTags/RetrieveGameplayTags.h"

UGA_Enemy_Acceleration::UGA_Enemy_Acceleration(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilityTags.AddTag(RetrieveGameplayTags::Ability_Enemy_SpecialAttack);
	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Enemy_SpecialAttack);

	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Dead);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Hit);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Groggy);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Attack);

	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = RetrieveGameplayTags::GameplayEvent_Enemy_SpecialAttack_Acceleration;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);

	CounteredEventTag = RetrieveGameplayTags::GameplayEvent_PatternCountered;
}

void UGA_Enemy_Acceleration::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
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

	UAnimMontage* Montage = ResolveMontage(TriggerEventData);
	if (!Montage)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bCountered = false;
	StartListeningForCountered();

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, Montage, MontagePlayRate, NAME_None, true);

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

void UGA_Enemy_Acceleration::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	if (CounterTask)
	{
		CounterTask->EndTask();
		CounterTask = nullptr;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

UAnimMontage* UGA_Enemy_Acceleration::ResolveMontage(const FGameplayEventData* TriggerEventData) const
{
	if (!TriggerEventData)
	{
		return nullptr;
	}

	return const_cast<UAnimMontage*>(Cast<UAnimMontage>(TriggerEventData->OptionalObject.Get()));
}

void UGA_Enemy_Acceleration::StartListeningForCountered()
{
	if (!CounteredEventTag.IsValid() || CounterTask)
	{
		return;
	}

	CounterTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		CounteredEventTag,
		nullptr,
		true,
		true);
	if (CounterTask)
	{
		CounterTask->EventReceived.AddDynamic(this, &ThisClass::OnCountered);
		CounterTask->ReadyForActivation();
	}
}

void UGA_Enemy_Acceleration::ApplyAccelerationEffect()
{
	if (bCountered || !AccelerationEffectClass || !HasAuthority(&GetCurrentActivationInfoRef()))
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return;
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);

	const FGameplayEffectSpecHandle SpecHandle =
		ASC->MakeOutgoingSpec(AccelerationEffectClass, GetAbilityLevel(), Context);
	if (SpecHandle.IsValid())
	{
		ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
}

void UGA_Enemy_Acceleration::FinishAbility(bool bWasCancelled)
{
	if (!IsActive())
	{
		return;
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
}

void UGA_Enemy_Acceleration::OnMontageCompleted()
{
	if (!bCountered)
	{
		ApplyAccelerationEffect();
	}

	FinishAbility(false);
}

void UGA_Enemy_Acceleration::OnMontageInterrupted()
{
	FinishAbility(true);
}

void UGA_Enemy_Acceleration::OnCountered(FGameplayEventData /*Payload*/)
{
	bCountered = true;

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->CurrentMontageStop(0.1f);
	}

	FinishAbility(true);
}
