#include "AbilitySystem/Enemy/GAS/GA_Enemy_WindRetreatSlash.h"

#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystem/Enemy/EnemyProjectile.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

UGA_Enemy_WindRetreatSlash::UGA_Enemy_WindRetreatSlash(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilityTags.Reset();
	ActivationOwnedTags.Reset();
	ActivationBlockedTags.Reset();
	AbilityTriggers.Reset();

	AbilityTags.AddTag(RetrieveGameplayTags::Ability_Enemy_SpecialAttack);
	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Enemy_SpecialAttack);

	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Dead);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Hit);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Groggy);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Attack);

	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = RetrieveGameplayTags::GameplayEvent_Enemy_SpecialAttack_WindRetreatSlash;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);
}

void UGA_Enemy_WindRetreatSlash::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RetreatTimerHandle);
	}

	RetreatElapsed = 0.f;
	bRetreatMovementPrepared = false;
	bRetreatMovementStarted = false;
	bProjectilesScheduled = false;

	if (RetreatStartEventTask)
	{
		RetreatStartEventTask->EndTask();
		RetreatStartEventTask = nullptr;
	}
	if (RetreatFireEventTask)
	{
		RetreatFireEventTask->EndTask();
		RetreatFireEventTask = nullptr;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_Enemy_WindRetreatSlash::OnSpecialAttackActivated()
{
	Super::OnSpecialAttackActivated();

	PrepareRetreatMovement();

	RetreatStartEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		RetrieveGameplayTags::GameplayEvent_Enemy_SpecialAttack_WindRetreatSlashStart,
		nullptr,
		true,
		true);
	if (!RetreatStartEventTask)
	{
		return;
	}

	RetreatStartEventTask->EventReceived.AddDynamic(this, &ThisClass::HandleRetreatStartEvent);
	RetreatStartEventTask->ReadyForActivation();

	RetreatFireEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		RetrieveGameplayTags::GameplayEvent_Enemy_SpecialAttack_WindRetreatSlashFire,
		nullptr,
		true,
		true);
	if (!RetreatFireEventTask)
	{
		return;
	}

	RetreatFireEventTask->EventReceived.AddDynamic(this, &ThisClass::HandleRetreatFireEvent);
	RetreatFireEventTask->ReadyForActivation();
}

void UGA_Enemy_WindRetreatSlash::HandleRetreatStartEvent(FGameplayEventData Payload)
{
	if (bRetreatMovementStarted)
	{
		return;
	}
	
	FaceTarget();
	ApplyJump();
	StartRetreatMovement();
}

void UGA_Enemy_WindRetreatSlash::HandleRetreatFireEvent(FGameplayEventData Payload)
{
	if (bProjectilesScheduled)
	{
		return;
	}

	FaceTarget();
	bProjectilesScheduled = true;
	ScheduleProjectiles(true);
}

void UGA_Enemy_WindRetreatSlash::OnSpecialAttackEnded()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RetreatTimerHandle);
	}

	if (RetreatStartEventTask)
	{
		RetreatStartEventTask->EndTask();
		RetreatStartEventTask = nullptr;
	}
	if (RetreatFireEventTask)
	{
		RetreatFireEventTask->EndTask();
		RetreatFireEventTask = nullptr;
	}

	Super::OnSpecialAttackEnded();
}

void UGA_Enemy_WindRetreatSlash::OnProjectileSpawned(AEnemyProjectile* Projectile, AActor* AvatarActor)
{
	Super::OnProjectileSpawned(Projectile, AvatarActor);

	if (!Projectile)
	{
		return;
	}

	Projectile->SetDamageMultiplier(ProjectileDamageMultiplier);
}

float UGA_Enemy_WindRetreatSlash::AdjustProjectileFireDelay(const float FireDelay, const int32 ProjectileIndex) const
{
	return FMath::Max(0.f, FireDelay);
}

void UGA_Enemy_WindRetreatSlash::PrepareRetreatMovement()
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor)
	{
		return;
	}
	
	RetreatStartLocation = AvatarActor->GetActorLocation();
	const FVector RetreatDirection = GetRetreatDirection();
	if (RetreatDirection.IsNearlyZero())
	{
		return;
	}

	RetreatFinalLocation = RetreatStartLocation + RetreatDirection * TotalRetreatDistance;
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation ProjectedLocation;
		const FVector ProjectionExtent(300.f, 300.f, 500.f);
		if (NavSys->ProjectPointToNavigation(RetreatFinalLocation, ProjectedLocation, ProjectionExtent))
		{
			RetreatFinalLocation = ProjectedLocation.Location;
		}
	}

	bRetreatMovementPrepared = true;
}

void UGA_Enemy_WindRetreatSlash::StartRetreatMovement()
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	if (!AvatarActor || !World)
	{
		return;
	}

	if (!bRetreatMovementPrepared)
	{
		PrepareRetreatMovement();
	}
	
	if (!bRetreatMovementPrepared)
	{
		return;
	}

	RetreatStartLocation = AvatarActor->GetActorLocation();
	const FVector RetreatDirection = GetRetreatDirection();
	if (RetreatDirection.IsNearlyZero())
	{
		return;
	}
	RetreatFinalLocation = RetreatStartLocation + RetreatDirection * TotalRetreatDistance;
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		FNavLocation ProjectedLocation;
		const FVector ProjectionExtent(300.f, 300.f, 500.f);
		if (NavSys->ProjectPointToNavigation(RetreatFinalLocation, ProjectedLocation, ProjectionExtent))
		{
			RetreatFinalLocation = ProjectedLocation.Location;
		}
	}
	RetreatFinalLocation.Z = RetreatStartLocation.Z;
	RetreatElapsed = 0.f;
	bRetreatMovementStarted = true;

	World->GetTimerManager().SetTimer(
		RetreatTimerHandle,
		this,
		&ThisClass::TickRetreat,
		RetreatTickInterval,
		true);
}

void UGA_Enemy_WindRetreatSlash::TickRetreat()
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor || !bRetreatMovementStarted)
	{
		return;
	}

	RetreatElapsed += RetreatTickInterval;
	const float Alpha = FMath::Clamp(RetreatElapsed / FMath::Max(RetreatMoveDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);
	const float SmoothAlpha = FMath::InterpEaseOut(0.f, 1.f, Alpha, 2.f);

	const FVector CurrentLocation = AvatarActor->GetActorLocation();
	FVector NewLocation = CurrentLocation;
	NewLocation.X = FMath::Lerp(RetreatStartLocation.X, RetreatFinalLocation.X, SmoothAlpha);
	NewLocation.Y = FMath::Lerp(RetreatStartLocation.Y, RetreatFinalLocation.Y, SmoothAlpha);

	AvatarActor->SetActorLocation(NewLocation, true);
	FaceTarget();

	if (Alpha >= 1.f)
	{
		FinishRetreatMovement();
	}
}

void UGA_Enemy_WindRetreatSlash::FinishRetreatMovement()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RetreatTimerHandle);
	}

	RetreatElapsed = 0.f;
	bRetreatMovementStarted = false;
	FaceTarget();
}

void UGA_Enemy_WindRetreatSlash::ApplyJump()
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		return;
	}

	Character->LaunchCharacter(FVector(0.f, 0.f, JumpVerticalVelocity), false, true);
}

void UGA_Enemy_WindRetreatSlash::FaceTarget() const
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	const AActor* TargetActor = GetCachedTargetActor();
	if (!AvatarActor || !TargetActor)
	{
		return;
	}

	FVector Direction = TargetActor->GetActorLocation() - AvatarActor->GetActorLocation();
	Direction.Z = 0.f;
	if (!Direction.IsNearlyZero())
	{
		AvatarActor->SetActorRotation(Direction.Rotation());
	}
}

FVector UGA_Enemy_WindRetreatSlash::GetRetreatDirection() const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	const AActor* TargetActor = GetCachedTargetActor();
	if (!AvatarActor || !TargetActor)
	{
		return FVector::ZeroVector;
	}

	FVector RetreatDirection = AvatarActor->GetActorLocation() - TargetActor->GetActorLocation();
	RetreatDirection.Z = 0.f;
	if (RetreatDirection.IsNearlyZero())
	{
		RetreatDirection = -AvatarActor->GetActorForwardVector();
		RetreatDirection.Z = 0.f;
	}
	RetreatDirection.Normalize();

	return RetreatDirection;
}
