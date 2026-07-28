#include "AbilitySystem/Enemy/GAS/GA_Enemy_SpawnGroundHazard.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "AbilitySystem/Enemy/EnemyGroundHazard.h"
#include "Components/Enemy/EnemyCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "TimerManager.h"

UGA_Enemy_SpawnGroundHazard::UGA_Enemy_SpawnGroundHazard(const FObjectInitializer& ObjectInitializer)
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
	TriggerData.TriggerTag = RetrieveGameplayTags::GameplayEvent_Enemy_SpecialAttack_GroundHazard;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);
}

void UGA_Enemy_SpawnGroundHazard::ActivateAbility(
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

	if (!GroundHazardClass || SpawnEntries.IsEmpty())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAnimMontage* Montage = ResolveAttackMontage(TriggerEventData);
	if (!Montage)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

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

	ScheduleGroundHazards();
}

void UGA_Enemy_SpawnGroundHazard::EndAbility(
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
	SpawnedGroundLocations.Reset();

	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	PendingSpawnCount = 0;
	bMontageFinished = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

UAnimMontage* UGA_Enemy_SpawnGroundHazard::ResolveAttackMontage(
	const FGameplayEventData* TriggerEventData) const
{
	return TriggerEventData
		? const_cast<UAnimMontage*>(Cast<UAnimMontage>(TriggerEventData->OptionalObject.Get()))
		: nullptr;
}

void UGA_Enemy_SpawnGroundHazard::ScheduleGroundHazards()
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
			SpawnGroundHazard(EntryIndex);
			continue;
		}

		FTimerDelegate SpawnDelegate;
		SpawnDelegate.BindUObject(this, &ThisClass::SpawnGroundHazard, EntryIndex);

		FTimerHandle TimerHandle;
		World->GetTimerManager().SetTimer(TimerHandle, SpawnDelegate, AdjustedDelay, false);
		SpawnTimerHandles.Add(TimerHandle);
	}

	TryFinishAbility();
}

void UGA_Enemy_SpawnGroundHazard::SpawnGroundHazard(int32 SpawnEntryIndex)
{
	if (!SpawnEntries.IsValidIndex(SpawnEntryIndex))
	{
		PendingSpawnCount = FMath::Max(0, PendingSpawnCount - 1);
		TryFinishAbility();
		return;
	}

	AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	if (Avatar && World && HasAuthority(&GetCurrentActivationInfoRef()))
	{
		FVector BaseLocation = Avatar->GetActorLocation();
		if (USkeletalMeshComponent* Mesh = Avatar->FindComponentByClass<USkeletalMeshComponent>())
		{
			if (!SpawnBoneName.IsNone() && Mesh->DoesSocketExist(SpawnBoneName))
			{
				BaseLocation = Mesh->GetSocketLocation(SpawnBoneName);
			}
		}

		const FEnemyGroundHazardSpawnEntry& SpawnEntry = SpawnEntries[SpawnEntryIndex];
		FVector RequestedLocation = BaseLocation
			+ Avatar->GetActorRotation().RotateVector(SpawnEntry.SpawnOffset);

		const int32 PlacementAttempts = bRandomSpawn && SpawnableRadius > 0.f
			? FMath::Max(1, MaximumPlacementAttempts)
			: 1;

		for (int32 AttemptIndex = 0; AttemptIndex < PlacementAttempts; ++AttemptIndex)
		{
			FVector CandidateLocation = RequestedLocation;
			if (bRandomSpawn && SpawnableRadius > 0.f)
			{
				const FVector2D RandomOffset = FMath::RandPointInCircle(SpawnableRadius);
				CandidateLocation += FVector(RandomOffset.X, RandomOffset.Y, 0.f);
			}

			FVector GroundLocation;
			if (!ResolveGroundLocation(CandidateLocation, GroundLocation)
				|| IsTooCloseToExistingHazard(GroundLocation))
			{
				continue;
			}

			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = Avatar;
			SpawnParams.Instigator = Cast<APawn>(Avatar);
			SpawnParams.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

			const FRotator SpawnRotation(0.f, Avatar->GetActorRotation().Yaw, 0.f);
			if (World->SpawnActor<AEnemyGroundHazard>(
				GroundHazardClass,
				GroundLocation,
				SpawnRotation,
				SpawnParams))
			{
				SpawnedGroundLocations.Add(GroundLocation);
			}
			break;
		}
	}

	PendingSpawnCount = FMath::Max(0, PendingSpawnCount - 1);
	TryFinishAbility();
}

bool UGA_Enemy_SpawnGroundHazard::ResolveGroundLocation(
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

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SpawnGroundHazard), false, Avatar);
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

	if (HitResult.ImpactNormal.Z < MinGroundNormalZ)
	{
		return false;
	}

	OutGroundLocation = HitResult.ImpactPoint + HitResult.ImpactNormal * GroundOffset;
	return true;
}

bool UGA_Enemy_SpawnGroundHazard::IsTooCloseToExistingHazard(const FVector& GroundLocation) const
{
	if (MinimumSpawnSpacing <= 0.f)
	{
		return false;
	}

	const float MinimumSpacingSquared = FMath::Square(MinimumSpawnSpacing);
	for (const FVector& ExistingLocation : SpawnedGroundLocations)
	{
		if (FVector::DistSquared2D(GroundLocation, ExistingLocation) < MinimumSpacingSquared)
		{
			return true;
		}
	}

	return false;
}

void UGA_Enemy_SpawnGroundHazard::TryFinishAbility()
{
	if (PendingSpawnCount <= 0 && bMontageFinished)
	{
		FinishAbility(false);
	}
}

void UGA_Enemy_SpawnGroundHazard::FinishAbility(bool bWasCancelled)
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}

void UGA_Enemy_SpawnGroundHazard::OnMontageCompleted()
{
	bMontageFinished = true;
	TryFinishAbility();
}

void UGA_Enemy_SpawnGroundHazard::OnMontageInterrupted()
{
	FinishAbility(true);
}
