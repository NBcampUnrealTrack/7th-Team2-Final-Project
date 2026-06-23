#include "AbilitySystem/Player/GA_BowShot.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "AbilitySystem/Player/StaffProjectile.h"
#include "Combat/RetrieveTargetingLibrary.h"
#include "Components/Combat/CombatReactionComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/Player/WeaponComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "TimerManager.h"

UGA_BowShot::UGA_BowShot()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_BowShot);
	Tags.AddTag(RetrieveGameplayTags::Ability_Type_Attack);
	SetAssetTags(Tags);

	bBlockActivationWhileAirborne = true;

	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dodging);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Knockdown);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dead);

	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Player_Attacking);
	ActivationOwnedTags.AddTag(RetrieveGameplayTags::Animation_Lock_Rotation);
}

bool UGA_BowShot::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
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
	
	if (WeaponComp->GetWeaponDataRef().WeaponTypeTag != RetrieveGameplayTags::Weapon_Type_Bow)
	{
		return false;
	}

	return ProjectileClass != nullptr;
}

void UGA_BowShot::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	CachedWeaponComponent = AvatarActor ? AvatarActor->FindComponentByClass<UWeaponComponent>() : nullptr;

	CachedElementTag = ResolveCurrentElementTag();
	CachedAimTarget = ResolveAimTarget();

	ScheduleProjectiles();
	PlayFireMontageThenEnd();
}

void UGA_BowShot::ScheduleProjectiles()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<float> Delays = FireDelays;
	if (Delays.IsEmpty())
	{
		Delays.Add(0.f);
	}

	for (const float FireDelay : Delays)
	{
		const float ClampedDelay = FMath::Max(0.f, FireDelay);
		if (ClampedDelay <= 0.f)
		{
			SpawnProjectile();
			continue;
		}

		FTimerHandle SpawnTimerHandle;
		World->GetTimerManager().SetTimer(SpawnTimerHandle, this, &UGA_BowShot::SpawnProjectile, ClampedDelay, false);
		SpawnTimerHandles.Add(SpawnTimerHandle);
	}
}

void UGA_BowShot::SpawnProjectile()
{
	if (!HasAuthority(&GetCurrentActivationInfoRef()))
	{
		return;
	}

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	if (!ProjectileClass || !IsValid(AvatarActor) || !IsValid(World))
	{
		return;
	}

	FRetrieveProjectileSpawnParams Params;
	Params.ProjectileClass = ProjectileClass;
	Params.Speed = ProjectileSpeed;
	Params.DamageMultiplier = DamageMultiplier;
	Params.HitReactType = HitReactType;
	Params.SpawnSocketName = SpawnSocketName;
	Params.SpawnOffset = SpawnOffset;
	Params.AttackTypeTag = RetrieveGameplayTags::Attack_Type_Normal;
	Params.ElementTag = CachedElementTag;
	Params.ElementStatusEffect = ElementStatusEffects.FindRef(CachedElementTag);
	Params.ChargeBonusEventTag = ChargeBonusEventTag;

	UMeshComponent* WeaponMesh = IsValid(CachedWeaponComponent) ? CachedWeaponComponent->GetPrimaryEquippedWeaponMesh() : nullptr;
	AStaffProjectile::SpawnConfigured(World, AvatarActor, GetAbilitySystemComponentFromActorInfo(), WeaponMesh, CachedAimTarget, Params);
}

void UGA_BowShot::PlayFireMontageThenEnd()
{
	UAnimMontage* Montage = FireMontage.LoadSynchronous();
	if (!IsValid(Montage))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, Montage, MontagePlayRate, NAME_None, /*bStopWhenAbilityEnds=*/true);
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

void UGA_BowShot::HandleMontageFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}

AActor* UGA_BowShot::ResolveAimTarget() const
{
	return URetrieveTargetingLibrary::ResolveAimTarget(GetAvatarActorFromActorInfo(),
		AimSearchRange, AimSearchHalfAngle, AimMaxVerticalDelta, AimRangeWeightRate);
}

void UGA_BowShot::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		for (FTimerHandle& SpawnTimerHandle : SpawnTimerHandles)
		{
			World->GetTimerManager().ClearTimer(SpawnTimerHandle);
		}
	}
	SpawnTimerHandles.Reset();

	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	CachedWeaponComponent = nullptr;
	CachedAimTarget = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
