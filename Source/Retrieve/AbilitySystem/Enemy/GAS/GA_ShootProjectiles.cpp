#include "AbilitySystem/Enemy/GAS/GA_ShootProjectiles.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystem/Enemy/EnemyProjectile.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/Enemy/EnemyCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "TimerManager.h"

UGA_ShootProjectiles::UGA_ShootProjectiles(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilityTags.AddTag(RetrieveGameplayTags::Ability_Enemy_SpecialAttack);
	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Enemy_SpecialAttack);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Dead);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Hit);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Groggy);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Attack);
	//ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_SpecialAttack);

	const FGameplayTag TriggerTags[] =
	{
		RetrieveGameplayTags::GameplayEvent_Enemy_SpecialAttack,
		RetrieveGameplayTags::GameplayEvent_Enemy_SpecialAttack_ProjectileRapid,
		RetrieveGameplayTags::GameplayEvent_Enemy_SpecialAttack_ProjectileHoming,
		RetrieveGameplayTags::GameplayEvent_Enemy_SpecialAttack_ProjectileSpread,
	};

	for (const FGameplayTag& TriggerTag : TriggerTags)
	{
		FAbilityTriggerData TriggerData;
		TriggerData.TriggerTag = TriggerTag;
		TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
		AbilityTriggers.Add(TriggerData);
	}
}

void UGA_ShootProjectiles::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GA_ShootProjectiles] CommitAbility failed Owner=%s"),
			*GetNameSafe(GetAvatarActorFromActorInfo()));
		
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	CachedTargetActor = TriggerEventData ? const_cast<AActor*>(TriggerEventData->Target.Get()) : nullptr;

	OnSpecialAttackActivated();

	UAnimMontage* Montage = const_cast<UAnimMontage*>(ResolveMontage(TriggerEventData));
	if (!Montage)
	{
		Montage = ResolveFallbackSequenceMontage();
	}
	const bool bHasMontage = Montage != nullptr;

	if (bHasMontage)
	{
		// 에픽 전용: 이전에 재생 중이던 동적 슬롯 애니메이션(부양/비행 등)을 정리한 뒤 몽타주 재생.
		// 일반/보스는 원본 동작 유지(슬롯 정리 없음).
		if (UsesProjectileCompletionGuard())
		{
			if (AActor* AvatarActor = GetAvatarActorFromActorInfo())
			{
				if (USkeletalMeshComponent* Mesh = AvatarActor->FindComponentByClass<USkeletalMeshComponent>())
				{
					if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
					{
						AnimInstance->StopSlotAnimation(0.05f, TEXT("DefaultSlot"));
					}
				}
			}
		}

		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, const_cast<UAnimMontage*>(Montage), GetAttackMontagePlayRate(1.f), NAME_None, true);
		
		if (MontageTask)
		{
			MontageTask->OnCompleted.AddDynamic(this, &UGA_ShootProjectiles::OnMontageCompleted);
			MontageTask->OnBlendOut.AddDynamic(this, &UGA_ShootProjectiles::OnMontageCompleted);
			MontageTask->OnInterrupted.AddDynamic(this, &UGA_ShootProjectiles::OnMontageInterrupted);
			MontageTask->OnCancelled.AddDynamic(this, &UGA_ShootProjectiles::OnMontageInterrupted);
			MontageTask->ReadyForActivation();
		}
	}

	if (ShouldScheduleProjectilesOnActivate())
	{
		ScheduleProjectiles(bHasMontage);
	}
}

void UGA_ShootProjectiles::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (HasPendingScheduledProjectiles())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GA_ShootProjectiles] EndAbility deferred while scheduled projectiles remain. Owner=%s Row=%s Cancelled=%d Fired=%d/%d"),
			*GetNameSafe(GetAvatarActorFromActorInfo()),
			*ActivePatternRowName.ToString(),
			bWasCancelled ? 1 : 0,
			ActiveProjectileSpawnIndex,
			ActiveProjectileCount);
		return;
	}

	if (UWorld* World = GetWorld())
	{
		for (FTimerHandle& SpawnTimerHandle : SpawnTimerHandles)
		{
			World->GetTimerManager().ClearTimer(SpawnTimerHandle);
		}
		World->GetTimerManager().ClearTimer(FinishTimerHandle);
	}
	SpawnTimerHandles.Reset();
	bWaitingForScheduledProjectiles = false;

	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	OnSpecialAttackEnded();

	CachedTargetActor = nullptr;
	ActiveProjectileSpeed = FallbackProjectileSpeed ;
	ActiveHitReactType = ERetrieveHitReactType::Flinch;
	ActiveLaunchKnockbackConfig = FMonsterLaunchKnockbackConfig();
	ActiveEffectTag = FGameplayTag();
	ActiveStatusEffectClass = nullptr;
	ActiveProjectileClass = nullptr;
	ActiveDamageMultiplier = 1.f;
	ActivePatternRowName = NAME_None;
	ActiveProjectileSpawnIndex = 0;
	ActiveProjectileCount = 0;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_ShootProjectiles::ScheduleProjectiles(bool bHasMontage)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	ResolveAndCacheActivePattern();

	const UEnemyCombatComponent* Combat = GetEnemyCombatComponent();
	if (Combat)
	{
		ActivePatternRowName = Combat->GetActivePatternRowName();
	}

	TArray<float> FireDelays = ActiveProjectileConfig.ProjectileFireDelays;
	if (FireDelays.IsEmpty())
	{
		FireDelays.Add(FallbackProjectileSpawnDelay);
	}
	ActiveProjectileSpawnIndex = 0;
	ActiveProjectileCount = FireDelays.Num();
	// 완료 가드는 에픽 전용. 일반/보스는 항상 false → EndAbility/OnMontage* 의 신규 분기가 무력화되어 원본 동작.
	bWaitingForScheduledProjectiles = UsesProjectileCompletionGuard() && ActiveProjectileCount > 0;

	float LastFireDelay = 0.f;
	for (int32 FireDelayIndex = 0; FireDelayIndex < FireDelays.Num(); ++FireDelayIndex)
	{
		const float FireDelay = FireDelays[FireDelayIndex];
		const float ClampedDelay = Combat ? Combat->GetAttackDelay(FireDelay) : FMath::Max(0.f, FireDelay);
		const float AdjustedDelay = AdjustProjectileFireDelay(ClampedDelay, FireDelayIndex);
		LastFireDelay = FMath::Max(LastFireDelay, AdjustedDelay);

		if (AdjustedDelay <= 0.f)
		{
			SpawnProjectile();
			continue;
		}

		FTimerHandle SpawnTimerHandle;
		World->GetTimerManager().SetTimer(SpawnTimerHandle, this, 
			&UGA_ShootProjectiles::SpawnProjectile, AdjustedDelay, false);
		SpawnTimerHandles.Add(SpawnTimerHandle);
	}

	// 일반/보스: 몽타주가 없을 때만 종료 타이머 설정(원본 동작). 몽타주가 있으면 몽타주 완료가 종료를 구동.
	// 에픽: 다중 투사체가 몽타주보다 오래 걸릴 수 있으므로 항상 종료 타이머로 안전망 확보.
	if (!bHasMontage || UsesProjectileCompletionGuard())
	{
		World->GetTimerManager().SetTimer(FinishTimerHandle, this, &UGA_ShootProjectiles::FinishAbility,
			LastFireDelay + 0.25f, false);
	}
}

void UGA_ShootProjectiles::SpawnProjectile()
{
	
	if (!HasAuthority(&GetCurrentActivationInfoRef()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[GA_ShootProjectiles] Spawn skipped: no authority"));
		return;
	}

	const TSubclassOf<AEnemyProjectile> ResolvedProjectileClass = ResolveProjectileClass();
	if (!ResolvedProjectileClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GA_ShootProjectiles] Spawn skipped: ProjectileClass is null. Owner=%s Row=%s Pattern=%s ActiveClass=%s FallbackClass=%s"),
			*GetNameSafe(GetAvatarActorFromActorInfo()),
			*ActivePatternRowName.ToString(),
			*StaticEnum<EProjectileSpawnPattern>()->GetNameStringByValue(static_cast<int64>(ActiveProjectileConfig.SpawnPattern)),
			*GetNameSafe(ActiveProjectileClass),
			*GetNameSafe(ProjectileClass));
		return;
	}

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	if (!AvatarActor || !World)
	{
		return;
	}
	const int32 ProjectileIndex = ActiveProjectileSpawnIndex++;

	OnBeforeProjectileSpawn();

	FVector SpawnLocation = AvatarActor->GetActorLocation() + AvatarActor->GetActorRotation().RotateVector(SpawnOffset);
	FVector Direction = AvatarActor->GetActorForwardVector();
	bool bSpawnRadialVolley = false;
	int32 RadialVolleyCount = 1;
	float RadialVolleyYawOffset = 0.f;

	if (USkeletalMeshComponent* Mesh = ResolveProjectileSpawnMesh(AvatarActor))
	{
		if (!SpawnSocketName.IsNone() && Mesh->DoesSocketExist(SpawnSocketName))
		{
			SpawnLocation = Mesh->GetSocketLocation(SpawnSocketName);
		}
	}

	switch (ActiveProjectileConfig.SpawnPattern)
	{
	case EProjectileSpawnPattern::RainFromAbove:
		if (CachedTargetActor)
		{
			const FVector TargetLocation = CachedTargetActor->GetActorLocation();
			const FVector2D RandomOffset = FMath::RandPointInCircle(ActiveProjectileConfig.RainSpawnRadius);
			SpawnLocation = TargetLocation + FVector(RandomOffset.X, RandomOffset.Y, ActiveProjectileConfig.RainSpawnHeight);
			Direction = FVector(0.f, 0.f, -1.f);
		}
		break;

	case EProjectileSpawnPattern::GroundPillar:
		if (CachedTargetActor)
		{
			const FVector TargetLocation = CachedTargetActor->GetActorLocation();
			const FVector2D RandomOffset = FMath::RandPointInCircle(ActiveProjectileConfig.RainSpawnRadius);
			SpawnLocation = TargetLocation + FVector(
				RandomOffset.X,
				RandomOffset.Y,
				-FMath::Max(0.f, ActiveProjectileConfig.GroundPillarSpawnDepth));
			Direction = FVector(0.f, 0.f, 1.f);
		}
		break;

	case EProjectileSpawnPattern::RadialSpread:
		{
			bSpawnRadialVolley = true;
			RadialVolleyCount = FMath::Max(1, ActiveProjectileConfig.RadialProjectileCount);
			RadialVolleyYawOffset = (ProjectileIndex % 2) * (360.f / static_cast<float>(RadialVolleyCount * 2));
			if (SpawnSocketName.IsNone())
			{
				SpawnLocation = AvatarActor->GetActorLocation() + FVector(0.f, 0.f, SpawnOffset.Z);
			}
		}
		break;

	case EProjectileSpawnPattern::FanSpread:
		if (CachedTargetActor)
		{
			FVector AimLocation = CachedTargetActor->GetActorLocation();

			if (const UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(CachedTargetActor->GetRootComponent()))
			{
				AimLocation = RootPrimitive->Bounds.Origin;
			}

			const FVector BaseDirection = (AimLocation - SpawnLocation).GetSafeNormal();
			const int32 FanCount = FMath::Max(1, ActiveProjectileCount);
			const float FanAlpha = FanCount > 1
				? static_cast<float>(ProjectileIndex) / static_cast<float>(FanCount - 1)
				: 0.5f;
			const float HalfAngle = FMath::Max(0.f, ActiveProjectileConfig.FanSpreadAngle) * 0.5f;
			const float YawOffset = FMath::Lerp(-HalfAngle, HalfAngle, FanAlpha);
			Direction = FRotator(0.f, YawOffset, 0.f).RotateVector(BaseDirection).GetSafeNormal();
		}
		break;

	case EProjectileSpawnPattern::Aimed:
	default:
		if (CachedTargetActor)
		{
			const FVector AimedDirection = ResolveAimedProjectileDirection(SpawnLocation, CachedTargetActor);
			if (!AimedDirection.IsNearlyZero())
			{
				Direction = AimedDirection;
			}
		}
		break;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = AvatarActor;
	SpawnParams.Instigator = Cast<APawn>(AvatarActor);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	auto SpawnOneProjectile = [&](const FVector& LaunchDirection, const float LaunchSpeed, const int32 SpawnIndex, const int32 SpawnCount)
	{
		const FRotator LaunchRotation = LaunchDirection.Rotation();
		AEnemyProjectile* Projectile = World->SpawnActor<AEnemyProjectile>(
			ResolvedProjectileClass, SpawnLocation, LaunchRotation, SpawnParams);
		if (!Projectile)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[GA_ShootProjectiles] SpawnActor failed. Owner=%s Class=%s Location=%s"),
				*GetNameSafe(AvatarActor),
				*GetNameSafe(ResolvedProjectileClass),
				*SpawnLocation.ToCompactString());
			return;
		}

		Projectile->Launch(LaunchDirection, LaunchSpeed);
		Projectile->SetHitReactType(ActiveHitReactType);
		Projectile->SetEffectTag(ActiveEffectTag);
		Projectile->SetLaunchKnockbackConfig(ActiveLaunchKnockbackConfig);
		Projectile->SetStatusEffectClass(ActiveStatusEffectClass);
		Projectile->SetDamageMultiplier(ActiveDamageMultiplier);
		Projectile->SetProjectileLifetime(ActiveProjectileConfig.ProjectileLifetime);
		Projectile->SetGravityScale(ActiveProjectileConfig.bUseGravity
			? ActiveProjectileConfig.ProjectileGravityScale : 0.f);
		
		if (ActiveProjectileConfig.bUseHoming)
		{
			Projectile->ConfigureHoming(
				CachedTargetActor,
				ActiveProjectileConfig.HomingStartDelay,
				ActiveProjectileConfig.HomingDuration,
				ActiveProjectileConfig.HomingStrength);
		}

		OnProjectileSpawned(Projectile, AvatarActor);

		UE_LOG(LogTemp, Display,
			TEXT("[GA_ShootProjectiles] Spawned projectile. Owner=%s Row=%s Class=%s Location=%s Direction=%s Speed=%.1f Index=%d/%d"),
			*GetNameSafe(AvatarActor),
			*ActivePatternRowName.ToString(),
			*GetNameSafe(ResolvedProjectileClass),
			*SpawnLocation.ToCompactString(),
			*LaunchDirection.ToCompactString(),
			LaunchSpeed,
			SpawnIndex + 1,
			SpawnCount);
	};

	if (bSpawnRadialVolley)
	{
		for (int32 VolleyIndex = 0; VolleyIndex < RadialVolleyCount; ++VolleyIndex)
		{
			const float VolleyAlpha = RadialVolleyCount > 1
				? static_cast<float>(VolleyIndex) / static_cast<float>(RadialVolleyCount - 1)
				: 0.5f;
			const float Yaw = RadialVolleyYawOffset
				+ (360.f / static_cast<float>(RadialVolleyCount)) * static_cast<float>(VolleyIndex);
			const float Pitch = FMath::Lerp(
				ActiveProjectileConfig.SpreadPitchMin,
				ActiveProjectileConfig.SpreadPitchMax,
				VolleyAlpha);
			const FVector VolleyDirection = FRotator(Pitch, Yaw, 0.f).Vector();
			const float LaunchSpeed = ActiveProjectileConfig.ProjectileSpeed * FMath::Lerp(
				ActiveProjectileConfig.SpreadSpeedMultiplierMin,
				ActiveProjectileConfig.SpreadSpeedMultiplierMax,
				VolleyAlpha);
			SpawnOneProjectile(VolleyDirection, LaunchSpeed, VolleyIndex, RadialVolleyCount);
		}
	}
	else
	{
		SpawnOneProjectile(Direction, ActiveProjectileConfig.ProjectileSpeed, ProjectileIndex, ActiveProjectileCount);
	}
}

USkeletalMeshComponent* UGA_ShootProjectiles::ResolveProjectileSpawnMesh(AActor* AvatarActor) const
{
	return AvatarActor ? AvatarActor->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
}

FVector UGA_ShootProjectiles::ResolveAimedProjectileDirection(
	const FVector& SpawnLocation,
	AActor* TargetActor) const
{
	if (!IsValid(TargetActor))
	{
		return FVector::ZeroVector;
	}

	FVector AimLocation = TargetActor->GetActorLocation();
	if (const UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(TargetActor->GetRootComponent()))
	{
		AimLocation = RootPrimitive->Bounds.Origin;
	}

	return (AimLocation - SpawnLocation).GetSafeNormal();
}

bool UGA_ShootProjectiles::ResolveAndCacheActivePattern()
{
	ActiveProjectileConfig = FMonsterProjectilePatternConfig();
	ActiveProjectileConfig.ProjectileSpeed = FallbackProjectileSpeed;
	ActiveHitReactType = ERetrieveHitReactType::Flinch;
	ActiveLaunchKnockbackConfig = FMonsterLaunchKnockbackConfig();
	ActiveEffectTag = FGameplayTag();
	ActiveStatusEffectClass = nullptr;
	ActiveProjectileClass = nullptr;
	ActivePatternRowName = NAME_None;

	const UEnemyCombatComponent* Combat = GetEnemyCombatComponent();
	if (Combat)
	{
		ActivePatternRowName = Combat->GetActivePatternRowName();
	}

	const bool bResolved = ResolveProjectilePattern(
		ActiveProjectileConfig, &ActiveHitReactType, &ActiveLaunchKnockbackConfig,
		&ActiveEffectTag, &ActiveStatusEffectClass, &ActiveProjectileClass, &ActiveDamageMultiplier);

	if (!bResolved)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GA_ShootProjectiles] Failed to resolve active projectile pattern. Owner=%s Row=%s"),
			*GetNameSafe(GetAvatarActorFromActorInfo()), *ActivePatternRowName.ToString());
	}

	return bResolved;
}

TSubclassOf<AEnemyProjectile> UGA_ShootProjectiles::ResolveProjectileClass() const
{
	if (ActiveProjectileClass)
	{
		return ActiveProjectileClass;
	}

	return ProjectileClass;
}

void UGA_ShootProjectiles::FinishAbility()
{
	bWaitingForScheduledProjectiles = false;
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

bool UGA_ShootProjectiles::HasPendingScheduledProjectiles() const
{
	return bWaitingForScheduledProjectiles
		&& ActiveProjectileSpawnIndex < ActiveProjectileCount;
}

bool UGA_ShootProjectiles::ResolveProjectilePattern(FMonsterProjectilePatternConfig& OutConfig,
	ERetrieveHitReactType* OutHitReactType,
	FMonsterLaunchKnockbackConfig* OutLaunchKnockbackConfig,
	FGameplayTag* OutEffectTag,
	TSubclassOf<UGameplayEffect>* OutStatusEffectClass,
	TSubclassOf<AEnemyProjectile>* OutProjectileClass,
	float* OutDamageMultiplier) const
{
	const UEnemyCombatComponent* CombatComponent = GetEnemyCombatComponent();

	if (!CombatComponent)
	{
		return false;
	}

	const UDataTable* PatternTable = CombatComponent->GetPatternTable();
	const FName RowName = CombatComponent->GetActivePatternRowName();

	if (!PatternTable || RowName.IsNone())
	{
		return false;
	}
	
	const FMonsterPatternRow* Row = GetActivePatternRow();

	if (!Row)
	{
		return false;
	}

	OutConfig = bOverrideProjectileConfig
		? ProjectileConfigOverride
		: Row->ProjectileConfig;
	if (OutHitReactType)
	{
		*OutHitReactType = Row->HitReactType;
	}
	if (OutLaunchKnockbackConfig)
	{
		*OutLaunchKnockbackConfig = Row->LaunchKnockbackConfig;
	}
	if (OutEffectTag)
	{
		*OutEffectTag = Row->EffectTag;
	}
	if (OutStatusEffectClass)
	{
		*OutStatusEffectClass = Row->StatusEffectClass;
	}
	if (OutProjectileClass)
	{
		*OutProjectileClass = Row->ProjectileClass;
	}
	if (OutDamageMultiplier)
	{
		*OutDamageMultiplier = FMath::Max(0.f, Row->DamageMultiplier);
	}

	if (OutConfig.ProjectileSpeed <= 0.f)
	{
		OutConfig.ProjectileSpeed = FallbackProjectileSpeed ;
	}

	return true;
}

const UAnimMontage* UGA_ShootProjectiles::ResolveMontage(const FGameplayEventData* TriggerEventData) const
{
	if (!TriggerEventData)
	{
		return nullptr;
	}

	const UObject* EventAnimation = TriggerEventData->OptionalObject.Get();
	if (const UAnimMontage* EventMontage = Cast<UAnimMontage>(EventAnimation))
	{
		return EventMontage;
	}

	const UAnimSequenceBase* EventSequence = Cast<UAnimSequenceBase>(EventAnimation);
	if (!EventSequence)
	{
		return nullptr;
	}

	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	const USkeletalMeshComponent* Mesh = AvatarActor
		? AvatarActor->FindComponentByClass<USkeletalMeshComponent>()
		: nullptr;
	const UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return nullptr;
	}

	return UAnimMontage::CreateSlotAnimationAsDynamicMontage(
		const_cast<UAnimSequenceBase*>(EventSequence),
		TEXT("DefaultSlot"),
		0.1f,
		0.15f,
		1.f,
		1);
}

void UGA_ShootProjectiles::OnMontageCompleted()
{
	if (HasPendingScheduledProjectiles())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GA_ShootProjectiles] Montage completed before all projectiles fired. Keeping ability alive. Owner=%s Row=%s Fired=%d/%d"),
			*GetNameSafe(GetAvatarActorFromActorInfo()),
			*ActivePatternRowName.ToString(),
			ActiveProjectileSpawnIndex,
			ActiveProjectileCount);
		return;
	}

	FinishAbility();
}

void UGA_ShootProjectiles::OnMontageInterrupted()
{
	if (HasPendingScheduledProjectiles())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GA_ShootProjectiles] Montage interrupted, but scheduled projectiles remain. Keeping ability alive. Owner=%s Row=%s Fired=%d/%d"),
			*GetNameSafe(GetAvatarActorFromActorInfo()),
			*ActivePatternRowName.ToString(),
			ActiveProjectileSpawnIndex,
			ActiveProjectileCount);
		return;
	}

	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}
