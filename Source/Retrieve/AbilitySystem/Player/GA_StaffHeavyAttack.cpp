#include "AbilitySystem/Player/GA_StaffHeavyAttack.h"

#include "AbilitySystemComponent.h"
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

UGA_StaffHeavyAttack::UGA_StaffHeavyAttack()
{
	bActivateForStaff = true;
	
	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_HeavyAttack);
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_HeavyAttack_Staff);
	Tags.AddTag(RetrieveGameplayTags::Ability_Type_Attack);
	SetAssetTags(Tags);
	
	ActivationOwnedTags.AddTag(RetrieveGameplayTags::Animation_Lock_Rotation);
}

bool UGA_StaffHeavyAttack::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const UWeaponComponent* WeaponComp = AvatarActor ? AvatarActor->FindComponentByClass<UWeaponComponent>() : nullptr;
	if (!IsValid(WeaponComp) || !WeaponComp->IsEquipped())
	{
		return false;
	}
	
	return WeaponComp->GetWeaponDataRef().StaffAttack.ProjectileClass != nullptr;
}

void UGA_StaffHeavyAttack::ExecuteHeavyEffect(const FGameplayTag& ConsumedElement)
{
	ExecuteOwnerCue(RetrieveGameplayTags::GameplayCue_Staff_Cast);

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	CachedWeaponComponent = AvatarActor ? AvatarActor->FindComponentByClass<UWeaponComponent>() : nullptr;
	if (!IsValid(CachedWeaponComponent))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	CachedWeaponData = CachedWeaponComponent->GetWeaponDataRef();
	
	CachedElementTag = ResolveCurrentElementTag();
	if (!CachedElementTag.IsValid() || CachedElementTag == RetrieveGameplayTags::Element_None)
	{
		CachedElementTag = ConsumedElement;
	}

	CachedAimTarget = ResolveAimTarget();

	ScheduleProjectiles();
	
	PlayHeavyMontageThenEnd();
}

void UGA_StaffHeavyAttack::ScheduleProjectiles()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<float> FireDelays = CachedWeaponData.StaffAttack.FireDelays;
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
		World->GetTimerManager().SetTimer(SpawnTimerHandle, this, &UGA_StaffHeavyAttack::SpawnProjectile, ClampedDelay, false);
		SpawnTimerHandles.Add(SpawnTimerHandle);
	}
}

void UGA_StaffHeavyAttack::SpawnProjectile()
{
	if (!HasAuthority(&GetCurrentActivationInfoRef()))
	{
		return;
	}

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	const FWeaponStaffAttack& StaffData = CachedWeaponData.StaffAttack;
	if (!StaffData.ProjectileClass || !IsValid(AvatarActor) || !IsValid(World))
	{
		return;
	}

	FRetrieveProjectileSpawnParams Params;
	Params.ProjectileClass = StaffData.ProjectileClass;
	Params.Speed = StaffData.ProjectileSpeed;
	Params.DamageMultiplier = StaffData.DamageMultiplier;
	Params.HitReactType = StaffData.HitReactType;
	Params.SpawnSocketName = StaffData.SpawnSocketName;
	Params.SpawnOffset = StaffData.SpawnOffset;
	Params.AttackTypeTag = RetrieveGameplayTags::Attack_Type_Heavy;
	Params.ElementTag = CachedElementTag;
	Params.ElementStatusEffect = StaffData.ElementStatusEffects.FindRef(CachedElementTag);
	Params.ChargeBonusEventTag = StaffData.ChargeBonusEventTag;

	UMeshComponent* WeaponMesh = IsValid(CachedWeaponComponent) ? CachedWeaponComponent->GetPrimaryEquippedWeaponMesh() : nullptr;
	AStaffProjectile::SpawnConfigured(World, AvatarActor, GetAbilitySystemComponentFromActorInfo(), WeaponMesh, CachedAimTarget, Params);
}

AActor* UGA_StaffHeavyAttack::ResolveAimTarget() const
{
	return URetrieveTargetingLibrary::ResolveAimTarget(GetAvatarActorFromActorInfo(),
		AimSearchRange, AimSearchHalfAngle, AimMaxVerticalDelta, AimRangeWeightRate);
}

void UGA_StaffHeavyAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		for (FTimerHandle& SpawnTimerHandle : SpawnTimerHandles)
		{
			World->GetTimerManager().ClearTimer(SpawnTimerHandle);
		}
	}
	SpawnTimerHandles.Reset();

	CachedWeaponComponent = nullptr;
	CachedAimTarget = nullptr;
	
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
