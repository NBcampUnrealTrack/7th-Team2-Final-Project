#include "AbilitySystem/Player/GA_StaffHeavyAttack.h"

#include "AbilitySystemComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "AbilitySystem/Player/StaffProjectile.h"
#include "Combat/RetrieveTargetingLibrary.h"
#include "Components/CombatReactionComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WeaponComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Player/RetrievePlayerState.h"
#include "TimerManager.h"

UGA_StaffHeavyAttack::UGA_StaffHeavyAttack()
{
	bActivateForStaff = true;
	
	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_HeavyAttack);
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_HeavyAttack_Staff);
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

	const TSubclassOf<AStaffProjectile> ProjectileClass = CachedWeaponData.StaffAttack.ProjectileClass;
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	if (!ProjectileClass || !IsValid(AvatarActor) || !IsValid(World))
	{
		return;
	}

	// 스폰 위치: 무기 메시 소켓 → 캐릭터 메시 소켓 → 액터+오프셋(왼손)
	const FName SpawnSocket = CachedWeaponData.StaffAttack.SpawnSocketName;
	FVector SpawnLocation = AvatarActor->GetActorLocation() + AvatarActor->GetActorRotation().RotateVector(CachedWeaponData.StaffAttack.SpawnOffset);

	bool bResolvedSocket = false;
	if (!SpawnSocket.IsNone())
	{
		if (UMeshComponent* WeaponMesh = IsValid(CachedWeaponComponent) ? CachedWeaponComponent->GetPrimaryEquippedWeaponMesh() : nullptr)
		{
			if (WeaponMesh->DoesSocketExist(SpawnSocket))
			{
				SpawnLocation = WeaponMesh->GetSocketLocation(SpawnSocket);
				bResolvedSocket = true;
			}
		}

		if (!bResolvedSocket)
		{
			if (const ACharacter* Char = Cast<ACharacter>(AvatarActor))
			{
				if (USkeletalMeshComponent* CharMesh = Char->GetMesh())
				{
					if (CharMesh->DoesSocketExist(SpawnSocket))
					{
						SpawnLocation = CharMesh->GetSocketLocation(SpawnSocket);
						bResolvedSocket = true;
					}
				}
			}
		}
	}

	// 발사 방향: 조준 타겟(중심 Bounds) → 없으면 컨트롤 회전 전방
	FVector Direction = AvatarActor->GetActorForwardVector();
	if (IsValid(CachedAimTarget))
	{
		FVector AimLocation = CachedAimTarget->GetActorLocation();
		if (const UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(CachedAimTarget->GetRootComponent()))
		{
			AimLocation = RootPrimitive->Bounds.Origin;
		}
		Direction = (AimLocation - SpawnLocation).GetSafeNormal();
	}
	else if (const ACharacter* SourceChar = Cast<ACharacter>(AvatarActor))
	{
		Direction = SourceChar->GetControlRotation().Vector();
	}

	if (Direction.IsNearlyZero())
	{
		Direction = AvatarActor->GetActorForwardVector();
	}

	const FRotator SpawnRotation = Direction.Rotation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = AvatarActor;
	SpawnParams.Instigator = Cast<APawn>(AvatarActor);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AStaffProjectile* Projectile = World->SpawnActor<AStaffProjectile>(ProjectileClass, SpawnLocation, SpawnRotation, SpawnParams);
	if (!Projectile)
	{
		return;
	}

	const FWeaponStaffAttack& StaffData = CachedWeaponData.StaffAttack;
	const TSubclassOf<UGameplayEffect> ElementStatusGE = StaffData.ElementStatusEffects.FindRef(CachedElementTag);

	Projectile->ConfigureAttack(
		GetAbilitySystemComponentFromActorInfo(),
		AvatarActor,
		StaffData.DamageMultiplier,
		StaffData.HitReactType,
		RetrieveGameplayTags::Attack_Type_Heavy,
		CachedElementTag,
		ElementStatusGE,
		StaffData.ChargeBonusEventTag);

	Projectile->Launch(Direction, StaffData.ProjectileSpeed);

	if (bDebugDraw)
	{
		DrawDebugLine(World, SpawnLocation, SpawnLocation + Direction * 500.f, FColor::Magenta, false, 1.5f, 0, 1.f);
	}
}

AActor* UGA_StaffHeavyAttack::ResolveAimTarget() const
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor))
	{
		return nullptr;
	}

	// 1) 락온 타겟 우선
	if (const UCombatReactionComponent* CombatReaction = AvatarActor->FindComponentByClass<UCombatReactionComponent>())
	{
		if (AActor* LockOnTarget = CombatReaction->GetLockOnTarget())
		{
			return LockOnTarget;
		}
	}

	// 2) 비락온 → 컨트롤 회전 기준 전방 콘 소프트락
	ACharacter* SourceChar = Cast<ACharacter>(AvatarActor);
	if (!IsValid(SourceChar))
	{
		return nullptr;
	}

	const FVector Aim = SourceChar->GetControlRotation().Vector();
	return URetrieveTargetingLibrary::FindBestTarget(
		SourceChar, AimSearchRange, AimSearchHalfAngle, Aim, AimMaxVerticalDelta, AimRangeWeightRate);
}

FGameplayTag UGA_StaffHeavyAttack::ResolveCurrentElementTag() const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	const APawn* AvatarPawn = Cast<APawn>(AvatarActor);
	const ARetrievePlayerState* RetrievePlayerState = AvatarPawn ? AvatarPawn->GetPlayerState<ARetrievePlayerState>() : nullptr;
	return RetrievePlayerState ? RetrievePlayerState->GetCurrentElementTag() : FGameplayTag();
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
