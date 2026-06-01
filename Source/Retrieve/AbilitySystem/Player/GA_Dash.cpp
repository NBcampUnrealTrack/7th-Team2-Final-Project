#include "AbilitySystem/Player/GA_Dash.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Components/RetrieveHeroComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"

UGA_Dash::UGA_Dash()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	
	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_Dash);
	SetAssetTags(Tags);

	// 회피 중 상태 태그
	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Player_Dodging);

	// 경직/다운/사망 중에는 발동 불가
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Knockdown);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dead);
	
	// 재대시 명시적 차단
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dodging);

	// 최상위 우선권 (공격/방어를 즉시 취소하고 발동)
	CancelAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Attack);
	CancelAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Guard);
}

void UGA_Dash::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!IsValid(AvatarActor))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	const FVector DashDir = ResolveDashDirection(ActorInfo);
	if (!DashDir.IsNearlyZero())
	{
		const FRotator YawOnly(0.f, DashDir.Rotation().Yaw, 0.f);
		AvatarActor->SetActorRotation(YawOnly, ETeleportType::TeleportPhysics);
	}
	
	UAnimMontage* Montage = DashMontage.LoadSynchronous();
	if (!IsValid(Montage))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, Montage, 1.f, NAME_None, /*bStopWhenAbilityEnds=*/true);
	if (!MontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageFinished);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageFinished);
	MontageTask->ReadyForActivation();
}

bool UGA_Dash::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	
	const ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UCharacterMovementComponent* MoveComp = Character ? Character->GetCharacterMovement() : nullptr;
	if (MoveComp && MoveComp->IsFalling() || Character->bPressedJump)
	{
		return false;
	}

	return true;
}

FVector UGA_Dash::ResolveDashDirection(const FGameplayAbilityActorInfo* ActorInfo) const
{
	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!IsValid(AvatarActor))
	{
		return FVector::ZeroVector;
	}
	
	if (const URetrieveHeroComponent* Hero = URetrieveHeroComponent::FindHeroComponent(AvatarActor))
	{
		FVector Cached = Hero->GetCachedMoveInputDirection();
		Cached.Z = 0.f;
		if (!Cached.IsNearlyZero())
		{
			return Cached.GetSafeNormal();
		}
	}
	
	FVector Forward = AvatarActor->GetActorForwardVector();
	Forward.Z = 0.f;
	return Forward.GetSafeNormal();
}

void UGA_Dash::HandleMontageFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}

void UGA_Dash::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
