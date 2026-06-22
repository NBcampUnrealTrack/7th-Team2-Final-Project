#include "AbilitySystem/Enemy/GAS/GA_Enemy_SpawnPillars.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "AbilitySystem/Enemy/EnemyPillar.h"
#include "Components/Enemy/EnemyCombatComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "TimerManager.h"

UGA_Enemy_SpawnPillars::UGA_Enemy_SpawnPillars(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilityTags.AddTag(RetrieveGameplayTags::Ability_Enemy_SpecialAttack);
	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Enemy_SpecialAttack);

	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Dead);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Hit);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Groggy);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Attack);
}

void UGA_Enemy_SpawnPillars::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
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

	const FMonsterPatternRow* PatternRow = GetActivePatternRow();
	UAnimMontage* Montage = ResolveAttackMontage(TriggerEventData);
	if (!PatternRow || !PillarClass || SpawnEntries.IsEmpty()
		|| !Montage || !CacheTargetLocation(TriggerEventData))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	ActiveLaunchKnockbackConfig = PatternRow->LaunchKnockbackConfig;

	PendingSpawnCount = SpawnEntries.Num();
	bMontageFinished = false;

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		Montage,
		GetAttackMontagePlayRate(MontagePlayRate),
		NAME_None,
		true);
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

	SchedulePillars();
}

void UGA_Enemy_SpawnPillars::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		for (FTimerHandle& TimerHandle : SpawnTimerHandles)
		{
			World->GetTimerManager().ClearTimer(TimerHandle);
		}
	}
	SpawnTimerHandles.Reset();

	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	CachedTargetActor = nullptr;
	CachedTargetLocation = FVector::ZeroVector;
	ActiveLaunchKnockbackConfig = FMonsterLaunchKnockbackConfig();
	PendingSpawnCount = 0;
	bHasCachedTargetLocation = false;
	bMontageFinished = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

UAnimMontage* UGA_Enemy_SpawnPillars::ResolveAttackMontage(
	const FGameplayEventData* TriggerEventData) const
{
	return TriggerEventData
		? const_cast<UAnimMontage*>(Cast<UAnimMontage>(TriggerEventData->OptionalObject.Get()))
		: nullptr;
}

bool UGA_Enemy_SpawnPillars::CacheTargetLocation(const FGameplayEventData* TriggerEventData)
{
	CachedTargetActor = TriggerEventData
		? const_cast<AActor*>(TriggerEventData->Target.Get())
		: nullptr;
	if (!IsValid(CachedTargetActor))
	{
		return false;
	}

	CachedTargetLocation = CachedTargetActor->GetActorLocation();
	bHasCachedTargetLocation = true;
	return true;
}

void UGA_Enemy_SpawnPillars::SchedulePillars()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		FinishAbility(true);
		return;
	}

	const UEnemyCombatComponent* Combat = GetEnemyCombatComponent();
	for (int32 EntryIndex = 0; EntryIndex < SpawnEntries.Num(); ++EntryIndex)
	{
		const float SpawnDelay = SpawnEntries[EntryIndex].SpawnDelay;
		const float AdjustedDelay = Combat
			? Combat->GetAttackDelay(SpawnDelay)
			: FMath::Max(0.f, SpawnDelay);

		if (AdjustedDelay <= 0.f)
		{
			SpawnPillar(EntryIndex);
			continue;
		}

		FTimerDelegate SpawnDelegate;
		SpawnDelegate.BindUObject(this, &ThisClass::SpawnPillar, EntryIndex);

		FTimerHandle TimerHandle;
		World->GetTimerManager().SetTimer(TimerHandle, SpawnDelegate, AdjustedDelay, false);
		SpawnTimerHandles.Add(TimerHandle);
	}

	TryFinishAbility();
}

void UGA_Enemy_SpawnPillars::SpawnPillar(int32 SpawnEntryIndex)
{
	if (!SpawnEntries.IsValidIndex(SpawnEntryIndex))
	{
		PendingSpawnCount = FMath::Max(0, PendingSpawnCount - 1);
		TryFinishAbility();
		return;
	}

	AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	if (Avatar && World && bHasCachedTargetLocation && HasAuthority(&GetCurrentActivationInfoRef()))
	{
		const FEnemyPillarSpawnEntry& SpawnEntry = SpawnEntries[SpawnEntryIndex];
		FVector RequestedLocation = CachedTargetLocation
			+ Avatar->GetActorRotation().RotateVector(SpawnEntry.SpawnOffset);

		if (SpawnEntry.bRandomSpawn && SpawnEntry.SpawnableRadius > 0.f)
		{
			const FVector2D RandomOffset = FMath::RandPointInCircle(SpawnEntry.SpawnableRadius);
			RequestedLocation += FVector(RandomOffset.X, RandomOffset.Y, 0.f);
		}

		FVector GroundLocation;
		if (ResolveGroundLocation(RequestedLocation, GroundLocation))
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = Avatar;
			SpawnParams.Instigator = Cast<APawn>(Avatar);
			SpawnParams.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

			const FRotator SpawnRotation(0.f, Avatar->GetActorRotation().Yaw, 0.f);
			AEnemyPillar* SpawnedPillar = World->SpawnActor<AEnemyPillar>(
				PillarClass,
				GroundLocation,
				SpawnRotation,
				SpawnParams);
			if (SpawnedPillar)
			{
				SpawnedPillar->SetLaunchKnockbackConfig(ActiveLaunchKnockbackConfig);
			}
		}
	}

	PendingSpawnCount = FMath::Max(0, PendingSpawnCount - 1);
	TryFinishAbility();
}

bool UGA_Enemy_SpawnPillars::ResolveGroundLocation(
	const FVector& RequestedLocation,
	FVector& OutGroundLocation) const
{
	const UWorld* World = GetWorld();
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!World || !Avatar)
	{
		return false;
	}

	const FVector TraceStart = RequestedLocation + FVector::UpVector * GroundTraceUpDistance;
	const FVector TraceEnd = RequestedLocation - FVector::UpVector * GroundTraceDownDistance;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SpawnPillar), false, Avatar);
	if (IsValid(CachedTargetActor))
	{
		QueryParams.AddIgnoredActor(CachedTargetActor);
	}

	FHitResult HitResult;
	if (!World->LineTraceSingleByChannel(
		HitResult,
		TraceStart,
		TraceEnd,
		ECC_Visibility,
		QueryParams))
	{
		return false;
	}

	OutGroundLocation = HitResult.ImpactPoint + HitResult.ImpactNormal * GroundOffset;
	return true;
}

void UGA_Enemy_SpawnPillars::TryFinishAbility()
{
	if (PendingSpawnCount <= 0 && bMontageFinished)
	{
		FinishAbility(false);
	}
}

void UGA_Enemy_SpawnPillars::FinishAbility(bool bWasCancelled)
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}

void UGA_Enemy_SpawnPillars::OnMontageCompleted()
{
	bMontageFinished = true;
	TryFinishAbility();
}

void UGA_Enemy_SpawnPillars::OnMontageInterrupted()
{
	FinishAbility(true);
}
