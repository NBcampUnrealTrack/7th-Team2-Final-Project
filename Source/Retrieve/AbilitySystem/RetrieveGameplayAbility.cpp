#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GameplayEffect.h"
#include "GameplayTags/RetrieveGameplayTags.h"

URetrieveGameplayAbility::URetrieveGameplayAbility(const FObjectInitializer& ObjectInitializer) : Super(
	ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ClientOrServer;
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;
}

void URetrieveGameplayAbility::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo,
                                           const FGameplayAbilitySpec& AbilitySpec)
{
	Super::OnAvatarSet(ActorInfo, AbilitySpec);
	TryActivateAbilityOnSpawn(ActorInfo, AbilitySpec);
}

void URetrieveGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	if (bAutoListenForParried)
	{
		StartListeningForParried();
	}
}

void URetrieveGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (ParriedTask)
	{
		ParriedTask->EndTask();
		ParriedTask = nullptr;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URetrieveGameplayAbility::StartListeningForParried()
{
	if (ParriedTask)
	{
		return;
	}

	ParriedTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, RetrieveGameplayTags::GameplayEvent_Parried,
		/*OptionalExternalTarget=*/nullptr, /*OnlyTriggerOnce=*/true, /*OnlyMatchExact=*/true);
	if (ParriedTask)
	{
		ParriedTask->EventReceived.AddDynamic(this, &URetrieveGameplayAbility::HandleParried);
		ParriedTask->ReadyForActivation();
	}
}

void URetrieveGameplayAbility::HandleParried(FGameplayEventData /*Payload*/)
{
	if (!IsActive())
	{
		return;
	}
	
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
		IsValid(ASC) && ParriedStaggerEffect && HasAuthority(&GetCurrentActivationInfoRef()))
	{
		FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
		Ctx.AddSourceObject(this);

		if (const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(ParriedStaggerEffect, 1.f, Ctx);
			Spec.IsValid())
		{
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}
	
	CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateCancelAbility=*/true);
}

void URetrieveGameplayAbility::TryActivateAbilityOnSpawn(const FGameplayAbilityActorInfo* ActorInfo,
                                                         const FGameplayAbilitySpec& AbilitySpec) const
{
	if (!ActorInfo || AbilitySpec.IsActive())
	{
		return;
	}

	if (ActivationPolicy != ERetrieveAbilityActivationPolicy::OnSpawn)
	{
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	const AActor* AvatarActor = ActorInfo->AvatarActor.Get();

	if (!ASC || !AvatarActor)
	{
		return;
	}

	if (AvatarActor->GetTearOff() || AvatarActor->GetLifeSpan() > 0.0f)
	{
		return;
	}

	const bool bIsLocalExecution =
		NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::LocalPredicted ||
		NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::LocalOnly;
	const bool bIsServerExecution =
		NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::ServerOnly ||
		NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	const bool bClientShouldActivate = ActorInfo->IsLocallyControlled() && bIsLocalExecution;
	const bool bServerShouldActivate = ActorInfo->IsNetAuthority() && bIsServerExecution;

	if (bClientShouldActivate || bServerShouldActivate)
	{
		ASC->TryActivateAbility(AbilitySpec.Handle);
	}
}
