#include "AbilitySystem/Enemy/GAS/GA_ShootProjectiles.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystem/Enemy/EnemyProjectile.h"
#include "Components/Enemy/EnemyCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
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

	const UAnimMontage* Montage = ResolveMontage(TriggerEventData);
	const bool bHasMontage = Montage != nullptr;
	
	if (bHasMontage)
	{
		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, const_cast<UAnimMontage*>(Montage), 1.f, NAME_None, true);
		
		if (MontageTask)
		{
			MontageTask->OnCompleted.AddDynamic(this, &UGA_ShootProjectiles::OnMontageCompleted);
			MontageTask->OnBlendOut.AddDynamic(this, &UGA_ShootProjectiles::OnMontageCompleted);
			MontageTask->OnInterrupted.AddDynamic(this, &UGA_ShootProjectiles::OnMontageInterrupted);
			MontageTask->OnCancelled.AddDynamic(this, &UGA_ShootProjectiles::OnMontageInterrupted);
			MontageTask->ReadyForActivation();
		}
	}

	ScheduleProjectiles(bHasMontage);
}

void UGA_ShootProjectiles::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		for (FTimerHandle& SpawnTimerHandle : SpawnTimerHandles)
		{
			World->GetTimerManager().ClearTimer(SpawnTimerHandle);
		}
		World->GetTimerManager().ClearTimer(FinishTimerHandle);
	}
	SpawnTimerHandles.Reset();

	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	CachedTargetActor = nullptr;
	ActiveProjectileSpeed = FallbackProjectileSpeed ;
	ActiveHitReactType = ERetrieveHitReactType::Flinch;
	ActiveLaunchKnockbackConfig = FMonsterLaunchKnockbackConfig();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_ShootProjectiles::ScheduleProjectiles(bool bHasMontage)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	ActiveProjectileConfig = FMonsterProjectilePatternConfig();
	ActiveProjectileConfig.ProjectileSpeed = FallbackProjectileSpeed ;
	ActiveHitReactType = ERetrieveHitReactType::Flinch;
	ActiveLaunchKnockbackConfig = FMonsterLaunchKnockbackConfig();

	ResolveProjectilePattern(ActiveProjectileConfig, &ActiveHitReactType, &ActiveLaunchKnockbackConfig);

	TArray<float> FireDelays = ActiveProjectileConfig.ProjectileFireDelays;
	if (FireDelays.IsEmpty())
	{
		FireDelays.Add(FallbackProjectileSpawnDelay);
	}

	float LastFireDelay = 0.f;
	for (const float FireDelay : FireDelays)
	{
		const float ClampedDelay = FMath::Max(0.f, FireDelay);
		LastFireDelay = FMath::Max(LastFireDelay, ClampedDelay);

		if (ClampedDelay <= 0.f)
		{
			SpawnProjectile();
			continue;
		}

		FTimerHandle SpawnTimerHandle;
		World->GetTimerManager().SetTimer(SpawnTimerHandle, this, 
			&UGA_ShootProjectiles::SpawnProjectile, ClampedDelay, false);
		SpawnTimerHandles.Add(SpawnTimerHandle);
	}

	if (!bHasMontage)
	{
		World->GetTimerManager().SetTimer(FinishTimerHandle, this, &UGA_ShootProjectiles::FinishAbility,
			LastFireDelay + 0.2f,false);
	}
}

void UGA_ShootProjectiles::SpawnProjectile()
{
	
	if (!HasAuthority(&GetCurrentActivationInfoRef()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[GA_ShootProjectiles] Spawn skipped: no authority"));
		return;
	}

	if (!ProjectileClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GA_ShootProjectiles] Spawn skipped: ProjectileClass is null"));
		return;
	}

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	if (!AvatarActor || !World)
	{
		return;
	}

	FVector SpawnLocation = AvatarActor->GetActorLocation() + AvatarActor->GetActorRotation().RotateVector(SpawnOffset);
	FRotator SpawnRotation = AvatarActor->GetActorRotation();

	if (USkeletalMeshComponent* Mesh = AvatarActor->FindComponentByClass<USkeletalMeshComponent>())
	{
		if (!SpawnSocketName.IsNone() && Mesh->DoesSocketExist(SpawnSocketName))
		{
			SpawnLocation = Mesh->GetSocketLocation(SpawnSocketName);
		}
	}

	FVector Direction = AvatarActor->GetActorForwardVector();
	if (CachedTargetActor)
	{
		FVector AimLocation = CachedTargetActor->GetActorLocation();

		if (const UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(CachedTargetActor->GetRootComponent()))
		{
			AimLocation = RootPrimitive->Bounds.Origin;
		}

		Direction = (AimLocation - SpawnLocation).GetSafeNormal();
		SpawnRotation = Direction.Rotation();
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = AvatarActor;
	SpawnParams.Instigator = Cast<APawn>(AvatarActor);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AEnemyProjectile* Projectile = World->SpawnActor<AEnemyProjectile>(
		ProjectileClass, SpawnLocation, SpawnRotation, SpawnParams);
	if (Projectile)
	{
		Projectile->Launch(Direction, ActiveProjectileConfig.ProjectileSpeed);
		Projectile->SetHitReactType(ActiveHitReactType);
		Projectile->SetLaunchKnockbackConfig(ActiveLaunchKnockbackConfig);
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
	}
}

void UGA_ShootProjectiles::FinishAbility()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

bool UGA_ShootProjectiles::ResolveProjectilePattern(FMonsterProjectilePatternConfig& OutConfig,
	ERetrieveHitReactType* OutHitReactType,
	FMonsterLaunchKnockbackConfig* OutLaunchKnockbackConfig) const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	const UEnemyCombatComponent* CombatComponent =
		AvatarActor ? AvatarActor->FindComponentByClass<UEnemyCombatComponent>() : nullptr;

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
	
	const FMonsterPatternRow* Row =
		PatternTable->FindRow<FMonsterPatternRow>(RowName, TEXT("UGA_ShootProjectiles"));

	if (!Row)
	{
		return false;
	}

	OutConfig = Row->ProjectileConfig;
	if (OutHitReactType)
	{
		*OutHitReactType = Row->HitReactType;
	}
	if (OutLaunchKnockbackConfig)
	{
		*OutLaunchKnockbackConfig = Row->LaunchKnockbackConfig;
	}

	if (OutConfig.ProjectileSpeed <= 0.f)
	{
		OutConfig.ProjectileSpeed = FallbackProjectileSpeed ;
	}

	return true;
}

const UAnimMontage* UGA_ShootProjectiles::ResolveMontage(const FGameplayEventData* TriggerEventData) const
{
	if (TriggerEventData)
	{
		if (const UAnimMontage* EventMontage = Cast<UAnimMontage>(TriggerEventData->OptionalObject.Get()))
		{
			return EventMontage;
		}
	}

	return nullptr;
}

void UGA_ShootProjectiles::OnMontageCompleted()
{
	FinishAbility();
}

void UGA_ShootProjectiles::OnMontageInterrupted()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}
