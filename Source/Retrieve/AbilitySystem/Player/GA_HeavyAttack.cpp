#include "AbilitySystem/Player/GA_HeavyAttack.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Components/MeshComponent.h"
#include "Components/Player/PlayerBurstComponent.h"
#include "Components/Player/WeaponComponent.h"
#include "Data/WeaponAttackDefinition.h"
#include "AbilitySystem/Player/StaffProjectile.h"
#include "Combat/RetrieveTargetingLibrary.h"
#include "Engine/World.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "TimerManager.h"

UGA_HeavyAttack::UGA_HeavyAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_HeavyAttack);
	Tags.AddTag(RetrieveGameplayTags::Ability_Type_Attack);
	SetAssetTags(Tags);

	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Player_UsingHeavyAttack);
	
	bUseCombatInputBuffer = true;
	CombatInputPriority = 10;

	bBlockActivationWhileAirborne = true;
	bBlockedByLocomotionAction = true;

	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dodging);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Knockdown);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dead);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_UsingHeavyAttack);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_ForcedKnockback);

	CancelAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Type_Attack);
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Guard);
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Dash);
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Burst);

	StaminaCostTag = RetrieveGameplayTags::Ability_Player_HeavyAttack;
}

bool UGA_HeavyAttack::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const UWeaponComponent* WeaponComp = AvatarActor ? AvatarActor->FindComponentByClass<UWeaponComponent>() : nullptr;
	if (!IsValid(WeaponComp) || !WeaponComp->IsEquipped())
	{
		return false;
	}
	
	return !WeaponComp->GetWeaponDataRef().AttackComboDefinition.IsNull();
}

void UGA_HeavyAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	CachedWeaponComponent = AvatarActor ? AvatarActor->FindComponentByClass<UWeaponComponent>() : nullptr;
	if (!IsValid(CachedWeaponComponent))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UWeaponAttackDefinition* ComboDef = CachedWeaponComponent->GetWeaponDataRef().AttackComboDefinition.LoadSynchronous();
	if (!IsValid(ComboDef))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	CachedElementTag = ResolveCurrentElementTag();
	const FWeaponHeavyAttack* Variant = ComboDef->ResolveHeavyVariant(CachedElementTag);
	if (!Variant)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GA_HeavyAttack] No heavy variant for element %s on %s"),
			*CachedElementTag.ToString(), *GetNameSafe(AvatarActor));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	CachedVariant = *Variant;

	ApplyCastLockTags();
	ExecuteOwnerCue(CachedVariant.ImpactCueTag);

	if (CachedVariant.AttackType == EAttackExecutionType::Projectile)
	{
		RunProjectilePath();
	}
	else
	{
		RunExecutorPath();
	}

	PlayMontageThenEnd();
}

void UGA_HeavyAttack::RunExecutorPath()
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	CachedBurstComp = AvatarActor ? AvatarActor->FindComponentByClass<UPlayerBurstComponent>() : nullptr;
	if (!IsValid(CachedBurstComp))
	{
		UE_LOG(LogTemp, Warning, TEXT("[GA_HeavyAttack] PlayerBurstComponent missing on %s; executor heavy deals no damage."),
			*GetNameSafe(AvatarActor));
		return;
	}

	FAttackExecutionSpec Spec;
	Spec.AttackType = CachedVariant.AttackType;
	Spec.DamageEffect = CachedVariant.DamageEffect;
	Spec.WorldSpawnActorClass = CachedVariant.WorldSpawnActorClass;
	Spec.WorldSpawnDistance = CachedVariant.WorldSpawnDistance;
	Spec.DashDistance = CachedVariant.DashDistance;
	Spec.DashLaunchDuration = CachedVariant.DashLaunchDuration;
	Spec.AoeRadius = CachedVariant.AoeRadius;
	Spec.ConeRadius = CachedVariant.ConeRadius;
	Spec.ConeHalfAngleDeg = CachedVariant.ConeHalfAngleDeg;
	Spec.HitSequence = CachedVariant.HitSequence;
	
	Spec.ElementTag = CachedElementTag;
	Spec.AttackTypeTag = RetrieveGameplayTags::Attack_Type_Heavy;
	Spec.AttackPropertyTag = RetrieveGameplayTags::Attack_Property_GuardBreak;
	Spec.HitEventTag = RetrieveGameplayTags::GameplayEvent_Hit_Heavy;

	bUsedBurstExecutor = true;
	// 돌진형 강공: 시전 중 적(Pawn) 통과 — 벽은 막힘. EndAbility에서 복구
	if (CachedVariant.AttackType == EAttackExecutionType::Dash)
	{
		SetAvatarPawnCollisionIgnored(true);
	}
	CachedBurstComp->BeginAttackExecution(Spec);
}

void UGA_HeavyAttack::RunProjectilePath()
{
	CachedAimTarget = ResolveAimTarget();
	ScheduleProjectiles();
}

void UGA_HeavyAttack::ScheduleProjectiles()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 다중 오프셋 모드(얼음창 좌/상/우 등): 오프셋 개수만큼 "동시에 스폰"하고, 투사체별 발사 지연은 FireDelays[i](없으면 ProjectileLaunchDelay).
	// → 3개가 머리 주변에 동시에 맺힌 뒤, 각자 다른 타이밍에 발사된다.
	if (CachedVariant.ProjectileSpawnOffsets.Num() > 0)
	{
		TArray<AStaffProjectile*> Volley;
		Volley.Reserve(CachedVariant.ProjectileSpawnOffsets.Num());
		for (int32 i = 0; i < CachedVariant.ProjectileSpawnOffsets.Num(); ++i)
		{
			const float LaunchDelay = CachedVariant.FireDelays.IsValidIndex(i)
				? FMath::Max(0.f, CachedVariant.FireDelays[i])
				: CachedVariant.ProjectileLaunchDelay;
			if (AStaffProjectile* Spawned = SpawnOneProjectile(CachedVariant.ProjectileSpawnOffsets[i], /*bAddOffsetToSocketBase=*/true, LaunchDelay))
			{
				Volley.Add(Spawned);
			}
		}

		// 먼저 발사된 창이 떠 있는 형제 창에 막혀 파괴되지 않도록 볼리끼리 상호 충돌 무시.
		for (int32 a = 0; a < Volley.Num(); ++a)
		{
			for (int32 b = 0; b < Volley.Num(); ++b)
			{
				if (a != b)
				{
					Volley[a]->IgnoreOtherProjectile(Volley[b]);
				}
			}
		}
		return;
	}

	// 레거시: 단일 SpawnOffset + FireDelays 개수만큼 순차 "스폰"(스폰 시 즉시 or ProjectileLaunchDelay 후 발사).
	TArray<float> FireDelays = CachedVariant.FireDelays;
	if (FireDelays.IsEmpty())
	{
		FireDelays.Add(0.f);
	}

	for (const float FireDelay : FireDelays)
	{
		const float ClampedDelay = FMath::Max(0.f, FireDelay);
		if (ClampedDelay <= 0.f)
		{
			SpawnProjectile();
			continue;
		}

		FTimerHandle SpawnTimerHandle;
		World->GetTimerManager().SetTimer(SpawnTimerHandle, this, &UGA_HeavyAttack::SpawnProjectile, ClampedDelay, false);
		SpawnTimerHandles.Add(SpawnTimerHandle);
	}
}

void UGA_HeavyAttack::SpawnProjectile()
{
	// 레거시 경로: 단일 SpawnOffset, 소켓 가산 없음, 발사지연=ProjectileLaunchDelay(기본 0=즉시).
	SpawnOneProjectile(CachedVariant.SpawnOffset, /*bAddOffsetToSocketBase=*/false, CachedVariant.ProjectileLaunchDelay);
}

AStaffProjectile* UGA_HeavyAttack::SpawnOneProjectile(FVector SpawnOffset, bool bAddOffsetToSocketBase, float LaunchDelay)
{
	if (!HasAuthority(&GetCurrentActivationInfoRef()))
	{
		return nullptr;
	}

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	if (!CachedVariant.ProjectileClass || !IsValid(AvatarActor) || !IsValid(World))
	{
		return nullptr;
	}

	if (!CachedVariant.ProjectileClass->IsChildOf(AStaffProjectile::StaticClass()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[GA_HeavyAttack] Projectile heavy: %s is not an AStaffProjectile subclass."),
			*GetNameSafe(CachedVariant.ProjectileClass));
		return nullptr;
	}

	FRetrieveProjectileSpawnParams Params;
	Params.ProjectileClass = CachedVariant.ProjectileClass.Get();
	Params.Speed = CachedVariant.ProjectileSpeed;
	Params.DamageMultiplier = CachedVariant.ProjectileDamageMultiplier;
	Params.HitReactType = CachedVariant.ProjectileHitReactType;
	Params.SpawnSocketName = CachedVariant.SpawnSocketName;
	Params.SpawnOffset = SpawnOffset;
	Params.bAddOffsetToSocketBase = bAddOffsetToSocketBase;
	Params.LaunchDelay = LaunchDelay;
	Params.bUseActorForward = CachedVariant.bLaunchInActorForward;
	Params.AttackTypeTag = RetrieveGameplayTags::Attack_Type_Heavy;
	Params.ElementTag = CachedElementTag;
	Params.ElementStatusEffect = CachedVariant.ProjectileElementStatusEffect;
	Params.ChargeBonusEventTag = CachedVariant.ChargeBonusEventTag;
	Params.LaunchSound = CachedVariant.ProjectileLaunchSound;

	UMeshComponent* WeaponMesh = IsValid(CachedWeaponComponent) ? CachedWeaponComponent->GetPrimaryEquippedWeaponMesh() : nullptr;
	return AStaffProjectile::SpawnConfigured(World, AvatarActor, GetAbilitySystemComponentFromActorInfo(), WeaponMesh, CachedAimTarget, Params);
}

AActor* UGA_HeavyAttack::ResolveAimTarget() const
{
	return URetrieveTargetingLibrary::ResolveAimTarget(GetAvatarActorFromActorInfo(),
		AimSearchRange, AimSearchHalfAngle, AimMaxVerticalDelta, AimRangeWeightRate);
}

void UGA_HeavyAttack::PlayMontageThenEnd()
{
	UAnimMontage* Montage = CachedVariant.Montage.LoadSynchronous();
	if (!IsValid(Montage))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
		return;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, Montage, CachedVariant.MontagePlayRate, NAME_None, /*bStopWhenAbilityEnds=*/true, 1.f, 0.f, /*bAllowInterruptAfterBlendOut=*/true);
	if (!MontageTask)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageFinished);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageFinished);
	MontageTask->ReadyForActivation();
}

void UGA_HeavyAttack::HandleMontageFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}

void UGA_HeavyAttack::ApplyCastLockTags()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(ASC))
	{
		return;
	}

	if (CachedVariant.bLockMovementDuringCast)
	{
		AppliedCastLockTags.AddTag(RetrieveGameplayTags::Animation_Lock_Movement);
	}
	if (CachedVariant.bLockRotationDuringCast)
	{
		AppliedCastLockTags.AddTag(RetrieveGameplayTags::Animation_Lock_Rotation);
	}

	for (const FGameplayTag& Tag : AppliedCastLockTags)
	{
		ASC->AddLooseGameplayTag(Tag);
	}
}

void UGA_HeavyAttack::RemoveCastLockTags()
{
	if (AppliedCastLockTags.IsEmpty())
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		for (const FGameplayTag& Tag : AppliedCastLockTags)
		{
			ASC->RemoveLooseGameplayTag(Tag);
		}
	}
	AppliedCastLockTags.Reset();
}

void UGA_HeavyAttack::ExecuteOwnerCue(const FGameplayTag& CueTag) const
{
	if (!CueTag.IsValid())
	{
		return;
	}
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		FGameplayCueParameters Params;
		Params.Instigator = GetAvatarActorFromActorInfo();
		ASC->ExecuteGameplayCue(CueTag, Params);
	}
}

void UGA_HeavyAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		for (FTimerHandle& SpawnTimerHandle : SpawnTimerHandles)
		{
			World->GetTimerManager().ClearTimer(SpawnTimerHandle);
		}
	}
	SpawnTimerHandles.Reset();

	RemoveCastLockTags();
	SetAvatarPawnCollisionIgnored(false);

	if (bUsedBurstExecutor && IsValid(CachedBurstComp))
	{
		CachedBurstComp->EndBurstSkill();
	}
	bUsedBurstExecutor = false;

	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	CachedWeaponComponent = nullptr;
	CachedBurstComp = nullptr;
	CachedAimTarget = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
