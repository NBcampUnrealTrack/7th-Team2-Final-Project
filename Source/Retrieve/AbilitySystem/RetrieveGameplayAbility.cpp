#include "AbilitySystem/RetrieveGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/Attributes/CombatAttributeSet.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Character/RetrieveAlsCharacter.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Player/RetrievePlayerState.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Settings/RetrieveStaminaSettings.h"
#include "AbilitySystem/Effects/RetrieveStaminaCostEffect.h"

URetrieveGameplayAbility::URetrieveGameplayAbility(const FObjectInitializer& ObjectInitializer) : Super(
	ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ClientOrServer;
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Swimming);
}

FGameplayTag URetrieveGameplayAbility::ResolveCurrentElementTag() const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	const APawn* AvatarPawn = Cast<APawn>(AvatarActor);
	const ARetrievePlayerState* RetrievePlayerState = AvatarPawn ? AvatarPawn->GetPlayerState<ARetrievePlayerState>() : nullptr;
	return RetrievePlayerState ? RetrievePlayerState->GetCurrentElementTag() : FGameplayTag();
}

FGameplayEffectSpecHandle URetrieveGameplayAbility::MakeSourcedSpec(TSubclassOf<UGameplayEffect> EffectClass, float Level) const
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(ASC) || !EffectClass)
	{
		return FGameplayEffectSpecHandle();
	}

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
	Ctx.AddInstigator(AvatarActor, AvatarActor);
	Ctx.AddSourceObject(this);

	return ASC->MakeOutgoingSpec(EffectClass, Level, Ctx);
}

TSubclassOf<UGameplayEffect> URetrieveGameplayAbility::SelectEffectByTargetType(const UAbilitySystemComponent* TargetASC, TSubclassOf<UGameplayEffect> NormalEffect, TSubclassOf<UGameplayEffect> BossEffect)
{
	const bool bIsBoss = TargetASC && TargetASC->HasMatchingGameplayTag(RetrieveGameplayTags::Monster_Type_Boss);
	return bIsBoss ? BossEffect : NormalEffect;
}

void URetrieveGameplayAbility::ApplyCommonActionBlocks(bool bBlockAirborne)
{
	bBlockActivationWhileAirborne = bBlockAirborne;
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dodging);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Knockdown);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dead);
}

void URetrieveGameplayAbility::SetAvatarPawnCollisionIgnored(bool bIgnore) const
{
	if (const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			// 적(Pawn) 관통 토글. 벽(WorldStatic/Dynamic)은 Block 유지.
			Capsule->SetCollisionResponseToChannel(ECC_Pawn, bIgnore ? ECR_Ignore : ECR_Block);
		}
	}
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

bool URetrieveGameplayAbility::GetStaminaCostRow(FStaminaCostRow& OutRow) const
{
	if (!StaminaCostTag.IsValid())
	{
		return false;
	}

	const URetrieveStaminaSettings* Settings = GetDefault<URetrieveStaminaSettings>();
	if (!Settings)
	{
		return false;
	}

	// 설정 맵(Project Settings > Retrieve > Stamina)에서 이 액션 태그의 비용을 조회. 없으면 무료.
	const FStaminaCostRow* Found = Settings->StaminaCosts.Find(StaminaCostTag);
	if (!Found)
	{
		return false;
	}

	OutRow = *Found;
	return true;
}

void URetrieveGameplayAbility::ApplyStaminaDelta(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, float Delta) const
{
	if (FMath::IsNearlyZero(Delta))
	{
		return;
	}

	FGameplayEffectSpecHandle Spec = MakeSourcedSpec(URetrieveStaminaCostEffect::StaticClass(), GetAbilityLevel());
	if (!Spec.IsValid() || !Spec.Data.IsValid())
	{
		return;
	}

	// 공용 GE 모디파이어(Stamina Additive)의 크기를 SetByCaller로 주입. 음수=소모, 양수=회복.
	Spec.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Cost_Stamina, Delta);
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
}

bool URetrieveGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags))
	{
		return false;
	}

	FStaminaCostRow Row;
	if (GetStaminaCostRow(Row))
	{
		const float Required = FMath::Max(Row.ActivationCost, Row.MinimumToActivate);
		if (Required > 0.f && !HasStamina(ActorInfo, Required))
		{
			return false;
		}
	}

	return true;
}

void URetrieveGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);

	FStaminaCostRow Row;
	if (GetStaminaCostRow(Row) && Row.ActivationCost > 0.f)
	{
		ApplyStaminaDelta(Handle, ActorInfo, ActivationInfo, -Row.ActivationCost);
	}
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
