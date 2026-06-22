#include "AbilitySystem/Enemy/GAS/GA_Enemy_ProjectileRain.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystem/Enemy/EnemyProjectile.h"
#include "Animation/AnimMontage.h"
#include "Components/DecalComponent.h"
#include "Components/Enemy/EnemyCombatComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"

UGA_Enemy_ProjectileRain::UGA_Enemy_ProjectileRain(
	const FObjectInitializer& ObjectInitializer)
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
	TriggerData.TriggerTag = RetrieveGameplayTags::GameplayEvent_Enemy_SpecialAttack_ProjectileRain;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);

	ReleaseEventTag = RetrieveGameplayTags::GameplayEvent_Attack_Impact_Begin;
}

void UGA_Enemy_ProjectileRain::ActivateAbility(
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
	if (!PatternRow || !Montage || !CacheTargetLocation(TriggerEventData))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveProjectileConfig = PatternRow->ProjectileConfig;
	ActiveLaunchKnockbackConfig = PatternRow->LaunchKnockbackConfig;
	ActiveHitReactType = PatternRow->HitReactType;
	ActiveEffectTag = PatternRow->EffectTag;
	ActiveStatusEffectClass = PatternRow->StatusEffectClass;
	ActiveProjectileClass = PatternRow->ProjectileClass
		? PatternRow->ProjectileClass
		: ProjectileClass;

	if (!ActiveProjectileClass || !BuildSpawnLocations())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ReleaseEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		ReleaseEventTag,
		nullptr,
		true,
		true);
	if (!ReleaseEventTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	ReleaseEventTask->EventReceived.AddDynamic(this, &ThisClass::HandleReleaseEvent);
	ReleaseEventTask->ReadyForActivation();

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

	SchedulePreparedProjectiles();
}

void UGA_Enemy_ProjectileRain::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		for (FTimerHandle& TimerHandle : PreparationTimerHandles)
		{
			World->GetTimerManager().ClearTimer(TimerHandle);
		}
	}
	PreparationTimerHandles.Reset();

	if (!bProjectilesReleased)
	{
		for (const TWeakObjectPtr<AEnemyProjectile>& ProjectilePtr : PreparedProjectiles)
		{
			if (AEnemyProjectile* Projectile = ProjectilePtr.Get())
			{
				Projectile->Destroy();
			}
		}
	}

	FadeWarningDecals();

	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}
	if (ReleaseEventTask)
	{
		ReleaseEventTask->EndTask();
		ReleaseEventTask = nullptr;
	}

	CachedTargetActor = nullptr;
	CachedTargetLocation = FVector::ZeroVector;
	GroundLocations.Reset();
	ProjectileSpawnLocations.Reset();
	PreparedProjectiles.Reset();
	WarningDecals.Reset();
	PreparedSpawnPointIndices.Reset();
	ActiveProjectileConfig = FMonsterProjectilePatternConfig();
	ActiveLaunchKnockbackConfig = FMonsterLaunchKnockbackConfig();
	ActiveHitReactType = ERetrieveHitReactType::Flinch;
	ActiveEffectTag = FGameplayTag();
	ActiveStatusEffectClass = nullptr;
	ActiveProjectileClass = nullptr;
	bProjectilesReleased = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

UAnimMontage* UGA_Enemy_ProjectileRain::ResolveAttackMontage(
	const FGameplayEventData* TriggerEventData) const
{
	return TriggerEventData
		? const_cast<UAnimMontage*>(Cast<UAnimMontage>(TriggerEventData->OptionalObject.Get()))
		: nullptr;
}

bool UGA_Enemy_ProjectileRain::CacheTargetLocation(
	const FGameplayEventData* TriggerEventData)
{
	CachedTargetActor = TriggerEventData
		? const_cast<AActor*>(TriggerEventData->Target.Get())
		: nullptr;
	if (!IsValid(CachedTargetActor))
	{
		return false;
	}

	CachedTargetLocation = CachedTargetActor->GetActorLocation();
	return true;
}

bool UGA_Enemy_ProjectileRain::BuildSpawnLocations()
{
	GroundLocations.Reset();
	ProjectileSpawnLocations.Reset();

	const int32 SpawnCount = ActiveProjectileConfig.ProjectileFireDelays.Num();
	if (SpawnCount <= 0)
	{
		return false;
	}

	const float CandidateRadius = FMath::Max(
		0.f,
		ActiveProjectileConfig.RainSpawnRadius - WarningRadius);
	const float MinimumSpacing = WarningRadius * 2.f + SpacingPadding;
	const float MinimumSpacingSquared = FMath::Square(MinimumSpacing);
	const int32 PlacementAttemptLimit = FMath::Max(
		MaximumPlacementAttempts,
		SpawnCount * 10);

	int32 AttemptCount = 0;
	while (GroundLocations.Num() < SpawnCount
		&& AttemptCount < PlacementAttemptLimit)
	{
		++AttemptCount;

		const FVector2D RandomOffset = FMath::RandPointInCircle(CandidateRadius);
		const FVector RequestedLocation = CachedTargetLocation
			+ FVector(RandomOffset.X, RandomOffset.Y, 0.f);

		FVector GroundLocation;
		if (!ResolveGroundLocation(RequestedLocation, GroundLocation))
		{
			continue;
		}

		bool bOverlapsExistingPoint = false;
		for (const FVector& ExistingLocation : GroundLocations)
		{
			if (FVector::DistSquared2D(GroundLocation, ExistingLocation)
				< MinimumSpacingSquared)
			{
				bOverlapsExistingPoint = true;
				break;
			}
		}
		if (bOverlapsExistingPoint)
		{
			continue;
		}

		FVector ProjectileSpawnLocation;
		if (!ResolveProjectileSpawnLocation(GroundLocation, ProjectileSpawnLocation))
		{
			continue;
		}

		GroundLocations.Add(GroundLocation);
		ProjectileSpawnLocations.Add(ProjectileSpawnLocation);
	}

	return !GroundLocations.IsEmpty();
}

bool UGA_Enemy_ProjectileRain::ResolveGroundLocation(
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

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ProjectileRainGround), false, Avatar);
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

bool UGA_Enemy_ProjectileRain::ResolveProjectileSpawnLocation(
	const FVector& GroundLocation,
	FVector& OutSpawnLocation) const
{
	const UWorld* World = GetWorld();
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!World || !Avatar)
	{
		return false;
	}

	const float DesiredSpawnHeight = ActiveProjectileConfig.RainSpawnHeight;
	float AvailableHeight = DesiredSpawnHeight;
	const FVector TraceStart = GroundLocation + FVector::UpVector * FMath::Max(1.f, GroundOffset);
	const FVector TraceEnd = GroundLocation + FVector::UpVector
		* (DesiredSpawnHeight + CeilingTraceExtraDistance);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ProjectileRainCeiling), false, Avatar);
	if (IsValid(CachedTargetActor))
	{
		QueryParams.AddIgnoredActor(CachedTargetActor);
	}

	FHitResult HitResult;
	if (World->LineTraceSingleByChannel(
		HitResult,
		TraceStart,
		TraceEnd,
		ECC_Visibility,
		QueryParams))
	{
		AvailableHeight = FMath::Min(
			DesiredSpawnHeight,
			FVector::Dist(GroundLocation, HitResult.ImpactPoint) - CeilingClearance);
	}

	if (AvailableHeight <= 0.f)
	{
		return false;
	}

	OutSpawnLocation = GroundLocation + FVector::UpVector * AvailableHeight;
	return true;
}

void UGA_Enemy_ProjectileRain::SchedulePreparedProjectiles()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		FinishAbility(true);
		return;
	}

	const UEnemyCombatComponent* Combat = GetEnemyCombatComponent();
	for (int32 SpawnPointIndex = 0; SpawnPointIndex < ProjectileSpawnLocations.Num(); ++SpawnPointIndex)
	{
		const float BaseDelay = ActiveProjectileConfig.ProjectileFireDelays[SpawnPointIndex];
		const float AdjustedDelay = Combat
			? Combat->GetAttackDelay(BaseDelay)
			: FMath::Max(0.f, BaseDelay);

		if (AdjustedDelay <= 0.f)
		{
			SpawnPreparedProjectile(SpawnPointIndex);
			continue;
		}

		FTimerDelegate SpawnDelegate;
		SpawnDelegate.BindUObject(
			this,
			&ThisClass::SpawnPreparedProjectile,
			SpawnPointIndex);

		FTimerHandle TimerHandle;
		World->GetTimerManager().SetTimer(TimerHandle, SpawnDelegate, AdjustedDelay, false);
		PreparationTimerHandles.Add(TimerHandle);
	}
}

void UGA_Enemy_ProjectileRain::SpawnPreparedProjectile(int32 SpawnPointIndex)
{
	if (!ProjectileSpawnLocations.IsValidIndex(SpawnPointIndex)
		|| !GroundLocations.IsValidIndex(SpawnPointIndex)
		|| PreparedSpawnPointIndices.Contains(SpawnPointIndex)
		|| !ActiveProjectileClass
		|| !HasAuthority(&GetCurrentActivationInfoRef()))
	{
		return;
	}

	AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	if (!Avatar || !World)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Avatar;
	SpawnParams.Instigator = Cast<APawn>(Avatar);
	SpawnParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AEnemyProjectile* Projectile = World->SpawnActor<AEnemyProjectile>(
		ActiveProjectileClass,
		ProjectileSpawnLocations[SpawnPointIndex],
		FRotator(-90.f, 0.f, 0.f),
		SpawnParams);
	if (!Projectile)
	{
		return;
	}

	Projectile->SetHitReactType(ActiveHitReactType);
	Projectile->SetEffectTag(ActiveEffectTag);
	Projectile->SetLaunchKnockbackConfig(ActiveLaunchKnockbackConfig);
	Projectile->SetStatusEffectClass(ActiveStatusEffectClass);
	Projectile->PrepareForDelayedLaunch();
	PreparedProjectiles.Add(Projectile);
	PreparedSpawnPointIndices.Add(SpawnPointIndex);

	if (WarningDecalMaterial)
	{
		UDecalComponent* WarningDecal = UGameplayStatics::SpawnDecalAtLocation(
			World,
			WarningDecalMaterial,
			FVector(WarningDecalDepth, WarningRadius, WarningRadius),
			GroundLocations[SpawnPointIndex],
			FRotator(-90.f, 0.f, 0.f));
		if (WarningDecal)
		{
			WarningDecals.Add(WarningDecal);
		}
	}

	if (bProjectilesReleased)
	{
		Projectile->ReleaseDelayedLaunch(
			FVector::DownVector,
			ActiveProjectileConfig.ProjectileSpeed,
			ActiveProjectileConfig.ProjectileLifetime,
			ActiveProjectileConfig.bUseGravity
				? ActiveProjectileConfig.ProjectileGravityScale
				: 0.f);
	}
}

void UGA_Enemy_ProjectileRain::HandleReleaseEvent(FGameplayEventData Payload)
{
	ReleasePreparedProjectiles();
}

void UGA_Enemy_ProjectileRain::ReleasePreparedProjectiles()
{
	if (bProjectilesReleased)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		for (FTimerHandle& TimerHandle : PreparationTimerHandles)
		{
			World->GetTimerManager().ClearTimer(TimerHandle);
		}
	}
	PreparationTimerHandles.Reset();

	for (int32 SpawnPointIndex = 0;
		SpawnPointIndex < ProjectileSpawnLocations.Num();
		++SpawnPointIndex)
	{
		if (!PreparedSpawnPointIndices.Contains(SpawnPointIndex))
		{
			SpawnPreparedProjectile(SpawnPointIndex);
		}
	}

	bProjectilesReleased = true;
	for (const TWeakObjectPtr<AEnemyProjectile>& ProjectilePtr : PreparedProjectiles)
	{
		if (AEnemyProjectile* Projectile = ProjectilePtr.Get())
		{
			Projectile->ReleaseDelayedLaunch(
				FVector::DownVector,
				ActiveProjectileConfig.ProjectileSpeed,
				ActiveProjectileConfig.ProjectileLifetime,
				ActiveProjectileConfig.bUseGravity
					? ActiveProjectileConfig.ProjectileGravityScale
					: 0.f);
		}
	}

	FadeWarningDecals();
}

void UGA_Enemy_ProjectileRain::FadeWarningDecals()
{
	for (const TWeakObjectPtr<UDecalComponent>& DecalPtr : WarningDecals)
	{
		if (UDecalComponent* Decal = DecalPtr.Get())
		{
			Decal->SetFadeOut(0.f, WarningDecalFadeOutDuration, true);
		}
	}
	WarningDecals.Reset();
}

void UGA_Enemy_ProjectileRain::OnMontageCompleted()
{
	FinishAbility(false);
}

void UGA_Enemy_ProjectileRain::OnMontageInterrupted()
{
	FinishAbility(true);
}

void UGA_Enemy_ProjectileRain::FinishAbility(bool bWasCancelled)
{
	if (IsActive())
	{
		EndAbility(
			CurrentSpecHandle,
			CurrentActorInfo,
			CurrentActivationInfo,
			true,
			bWasCancelled);
	}
}
