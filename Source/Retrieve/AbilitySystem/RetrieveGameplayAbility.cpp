#include "AbilitySystem/RetrieveGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/Attributes/CombatAttributeSet.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Character/RetrieveAlsCharacter.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Player/RetrievePlayerState.h"

URetrieveGameplayAbility::URetrieveGameplayAbility(const FObjectInitializer& ObjectInitializer) : Super(
	ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ClientOrServer;
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;
}

FGameplayTag URetrieveGameplayAbility::ResolveCurrentElementTag() const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	const APawn* AvatarPawn = Cast<APawn>(AvatarActor);
	const ARetrievePlayerState* RetrievePlayerState = AvatarPawn ? AvatarPawn->GetPlayerState<ARetrievePlayerState>() : nullptr;
	return RetrievePlayerState ? RetrievePlayerState->GetCurrentElementTag() : FGameplayTag();
}

bool URetrieveGameplayAbility::HasStamina(const FGameplayAbilityActorInfo* ActorInfo, float Cost) const
{
	if (Cost <= 0.f)
	{
		return true;
	}

	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC)
	{
		return false;
	}

	return ASC->GetNumericAttribute(UCombatAttributeSet::GetStaminaAttribute()) >= Cost;
}

void URetrieveGameplayAbility::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo,
                                           const FGameplayAbilitySpec& AbilitySpec)
{
	Super::OnAvatarSet(ActorInfo, AbilitySpec);
	TryActivateAbilityOnSpawn(ActorInfo, AbilitySpec);
}

bool URetrieveGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	if (bBlockActivationWhileAirborne && IsAvatarAirborne(ActorInfo))
	{
		return false;
	}

	// ALS 액션(구르기/맨틀 등) 진행 중이면 차단. 단, 캔슬 윈도우(AttackCancelWindow)가 이 어빌리티의
	// 입력 intent를 허용하면 예외 — 캔슬 윈도우가 LocomotionAction 차단을 이긴다(ANS 겹침 시 캔슬 우선).
	// (LocomotionAction은 ASC 태그가 아니라 ALS API라 캐릭터에 위임. 캔슬 허용은 IsAttackCancelIntentAllowed로 조회)
	if (bBlockedByLocomotionAction)
	{
		if (const ARetrieveAlsCharacter* AlsCharacter = Cast<ARetrieveAlsCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
		{
			if (AlsCharacter->IsLocomotionActionActive() && !IsAllowedByActiveCancelWindow(ActorInfo, Handle))
			{
				return false;
			}
		}
	}

	return true;
}

bool URetrieveGameplayAbility::IsAllowedByActiveCancelWindow(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpecHandle Handle) const
{
	URetrieveAbilitySystemComponent* RetrieveASC = Cast<URetrieveAbilitySystemComponent>(ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr);
	if (!IsValid(RetrieveASC))
	{
		return false;
	}

	// 이 어빌리티의 입력 intent(DynamicSpecSourceTags의 입력 태그) 중 하나라도 현재 열린 캔슬 윈도우가
	// 허용하면 LocomotionAction 차단을 무시한다(캔슬 윈도우 우선).
	const FGameplayAbilitySpec* Spec = RetrieveASC->FindAbilitySpecFromHandle(Handle);
	if (!Spec)
	{
		return false;
	}

	for (const FGameplayTag& IntentTag : Spec->GetDynamicSpecSourceTags())
	{
		if (RetrieveASC->IsAttackCancelIntentAllowed(IntentTag))
		{
			return true;
		}
	}
	return false;
}

bool URetrieveGameplayAbility::IsAvatarAirborne(const FGameplayAbilityActorInfo* ActorInfo)
{
	const ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Character)
	{
		return false;
	}

	const UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
	return (MoveComp && MoveComp->IsFalling()) || Character->bPressedJump;
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
