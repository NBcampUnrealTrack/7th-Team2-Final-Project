#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "AbilitySystemComponent.h"

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
