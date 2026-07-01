#include "AbilitySystem/Enemy/GAS/GA_Enemy_SpawnWaveHazard.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystem/Enemy/EnemyWaveHazard.h"
#include "Animation/AnimMontage.h"
#include "Components/Enemy/EnemyCombatComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "TimerManager.h"

UGA_Enemy_SpawnWaveHazard::UGA_Enemy_SpawnWaveHazard(const FObjectInitializer& ObjectInitializer)
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
	TriggerData.TriggerTag = RetrieveGameplayTags::GameplayEvent_Enemy_SpecialAttack_Wave;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);
}

void UGA_Enemy_SpawnWaveHazard::ActivateAbility(
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

	UAnimMontage* Montage = ResolveAttackMontage(TriggerEventData);
	if (!WaveHazardClass || !Montage)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bWaveSpawned = false;
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

	ScheduleWaveSpawn();
}

void UGA_Enemy_SpawnWaveHazard::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpawnTimerHandle);
	}

	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	bWaveSpawned = false;
	bMontageFinished = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

UAnimMontage* UGA_Enemy_SpawnWaveHazard::ResolveAttackMontage(
	const FGameplayEventData* TriggerEventData) const
{
	return TriggerEventData
		? const_cast<UAnimMontage*>(Cast<UAnimMontage>(TriggerEventData->OptionalObject.Get()))
		: nullptr;
}

void UGA_Enemy_SpawnWaveHazard::ScheduleWaveSpawn()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		FinishAbility(true);
		return;
	}

	const UEnemyCombatComponent* Combat = GetEnemyCombatComponent();
	const float AdjustedDelay = Combat
		? Combat->GetAttackDelay(SpawnDelay)
		: FMath::Max(0.f, SpawnDelay);

	if (AdjustedDelay <= 0.f)
	{
		SpawnWave();
		return;
	}

	World->GetTimerManager().SetTimer(
		SpawnTimerHandle,
		this,
		&ThisClass::SpawnWave,
		AdjustedDelay,
		false);
}

void UGA_Enemy_SpawnWaveHazard::SpawnWave()
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	if (Avatar && World && HasAuthority(&GetCurrentActivationInfoRef()))
	{
		const FVector RequestedLocation = Avatar->GetActorLocation()
			+ Avatar->GetActorRotation().RotateVector(SpawnOffset);

		FVector GroundLocation;
		if (ResolveGroundLocation(RequestedLocation, GroundLocation))
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = Avatar;
			SpawnParams.Instigator = Cast<APawn>(Avatar);
			SpawnParams.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

			const FRotator SpawnRotation(0.f, Avatar->GetActorRotation().Yaw, 0.f);
			World->SpawnActor<AEnemyWaveHazard>(
				WaveHazardClass,
				GroundLocation,
				SpawnRotation,
				SpawnParams);
		}
	}

	bWaveSpawned = true;
	TryFinishAbility();
}

bool UGA_Enemy_SpawnWaveHazard::ResolveGroundLocation(
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

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SpawnWaveHazard), false, Avatar);

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

void UGA_Enemy_SpawnWaveHazard::TryFinishAbility()
{
	if (bWaveSpawned && bMontageFinished)
	{
		FinishAbility(false);
	}
}

void UGA_Enemy_SpawnWaveHazard::FinishAbility(bool bWasCancelled)
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}

void UGA_Enemy_SpawnWaveHazard::OnMontageCompleted()
{
	bMontageFinished = true;
	TryFinishAbility();
}

void UGA_Enemy_SpawnWaveHazard::OnMontageInterrupted()
{
	FinishAbility(true);
}
