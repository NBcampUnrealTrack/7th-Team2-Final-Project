#include "AbilitySystem/Player/GA_Blink.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "AbilitySystemComponent.h"
#include "Character/RetrieveAlsCharacter.h"
#include "Components/Player/RetrieveHeroComponent.h"
#include "Components/Player/WeaponComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "GameplayTags/RetrieveGameplayTags.h"

UGA_Blink::UGA_Blink()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_Dash);
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_Blink);
	SetAssetTags(Tags);

	bBlockActivationWhileAirborne = true;

	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dead);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Knockdown);

	CancelAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Type_Attack);

	StaminaCostTag = RetrieveGameplayTags::Ability_Player_Blink;
}

bool UGA_Blink::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const UWeaponComponent* WeaponComp = AvatarActor ? AvatarActor->FindComponentByClass<UWeaponComponent>() : nullptr;
	return WeaponComp &&
		WeaponComp->GetWeaponDataRef().WeaponTypeTag == RetrieveGameplayTags::Weapon_Type_Staff;
}

void UGA_Blink::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	bFallHandoff = false;

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ACharacter* Character = Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!Character || !World)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 출발 큐는 월드 위치라 이후 메시 은닉과 무관하게 출발 지점에 남는다.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		FGameplayCueParameters CueParams;
		CueParams.Instigator = Character;
		CueParams.Location = Character->GetActorLocation();
		ASC->ExecuteGameplayCue(RetrieveGameplayTags::GameplayCue_Blink, CueParams);
	}

	// 이동 과정을 감춘다. 착지 시 표시, 안 잡히면 타이머가 복원.
	if (ARetrieveAlsCharacter* AlsChar = Cast<ARetrieveAlsCharacter>(Character))
	{
		AlsChar->BeginBlink(BlinkFallMaxDuration);
	}

	FVector Dir = ResolveBlinkDirection(ActorInfo);
	if (Dir.IsNearlyZero())
	{
		Dir = Character->GetActorForwardVector();
	}
	Dir.Z = 0.f;
	Dir = Dir.GetSafeNormal();

	// 정면 고속 이동: Walking 모드라 지형을 타고 오르막을 오르고, 벽·적은 물리 충돌이 앞에서 멈춰준다.
	const FVector Start = Character->GetActorLocation();
	const FVector ForwardTarget(Start.X + Dir.X * BlinkDistance, Start.Y + Dir.Y * BlinkDistance, Start.Z);
	const float Duration = FMath::Max(0.02f, BlinkDistance / DashSpeed);

	DashTask = UAbilityTask_ApplyRootMotionMoveToForce::ApplyRootMotionMoveToForce(
		this, FName("BlinkForward"), ForwardTarget, Duration,
		/*bSetNewMovementMode=*/false, MOVE_Walking,
		/*bRestrictSpeedToExpected=*/true, /*PathOffsetCurve=*/nullptr,
		ERootMotionFinishVelocityMode::ClampVelocity, FVector::ZeroVector, /*ClampVelocityOnFinish=*/0.f);

	if (!DashTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	DashTask->OnTimedOut.AddDynamic(this, &ThisClass::OnForwardFinished);
	DashTask->OnTimedOutAndDestinationReached.AddDynamic(this, &ThisClass::OnForwardFinished);
	DashTask->ReadyForActivation();
}

void UGA_Blink::OnForwardFinished()
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!IsValid(Character))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	if (ARetrieveAlsCharacter* AlsChar = Cast<ARetrieveAlsCharacter>(Character))
	{
		const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
		if (Movement && Movement->IsFalling())
		{
			// 내리막: 중력을 키워 바닥까지 고속 낙하(착지 시 원복).
			AlsChar->SetBlinkFallGravity(BlinkFallGravityScale);
			bFallHandoff = true;
		}
		else
		{
			AlsChar->EndBlink();
		}
	}

	FinishBlink(Character);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Blink::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (DashTask)
	{
		DashTask->EndTask();
		DashTask = nullptr;
	}

	// 낙하 핸드오프 없이 끝나는 경우(평지/취소/실패)만 여기서 복원. 낙하 중이면 착지가 복원.
	if (!bFallHandoff)
	{
		if (ARetrieveAlsCharacter* AlsChar = Cast<ARetrieveAlsCharacter>(GetAvatarActorFromActorInfo()))
		{
			AlsChar->EndBlink();
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

FVector UGA_Blink::ResolveBlinkDirection(const FGameplayAbilityActorInfo* ActorInfo) const
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

void UGA_Blink::FinishBlink(ACharacter* Character)
{
	if (!IsValid(Character))
	{
		return;
	}

	// 도착 후 수평 관성으로 미끄러지지 않게 정리.
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->Velocity = FVector::ZeroVector;
	}
}
