#include "Components/PlayerBurstComponent.h"

#include "AbilitySystem/Player/BurstProjectile.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/RetrieveWeaponSockets.h"
#include "Components/MeshComponent.h"
#include "Components/WeaponComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Logging/RetrieveLogChannels.h"

UPlayerBurstComponent::UPlayerBurstComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPlayerBurstComponent::BeginBurstSkill(const FSkillCombination* Row)
{
	if (!Row)
	{
		UE_LOG(LogRetrieveCombat, Warning,
			TEXT("[PlayerBurstComponent] BeginBurstSkill called with null Row. Owner=%s"),
			*GetNameSafe(GetOwner()));
		return;
	}

	ActiveSkill = Row;
	SpawnedWorldActor.Reset();

	const int32 HitCount = Row->HitSequence.Num();
	PerHitHitActors.SetNum(HitCount);
	PerHitPreviousOrigin.SetNum(HitCount);
	PerHitHasPrevious.SetNum(HitCount);
	PerHitProjectileSpawned.SetNum(HitCount);
	for (int32 Index = 0; Index < HitCount; ++Index)
	{
		PerHitHitActors[Index].Reset();
		PerHitPreviousOrigin[Index] = FVector::ZeroVector;
		PerHitHasPrevious[Index] = false;
		PerHitProjectileSpawned[Index] = false;
	}

	UE_LOG(LogRetrieveCombat, Log,
		TEXT("[PlayerBurstComponent] BeginBurstSkill. Owner=%s, ElementPatternSize=%d"),
		*GetNameSafe(GetOwner()),
		Row->ElementPattern.Num());
}

void UPlayerBurstComponent::EndBurstSkill()
{
	UE_LOG(LogRetrieveCombat, Log,
		TEXT("[PlayerBurstComponent] EndBurstSkill. Owner=%s"),
		*GetNameSafe(GetOwner()));

	ActiveSkill = nullptr;
	SpawnedWorldActor.Reset();
	PerHitHitActors.Reset();
	PerHitPreviousOrigin.Reset();
	PerHitHasPrevious.Reset();
	PerHitProjectileSpawned.Reset();
}

void UPlayerBurstComponent::OnBurstHit(int32 HitIndex)
{
	if (!ActiveSkill)
	{
		UE_LOG(LogRetrieveCombat, Warning,
			TEXT("[PlayerBurstComponent] OnBurstHit(%d) called with no active skill. Owner=%s"),
			HitIndex,
			*GetNameSafe(GetOwner()));
		return;
	}

	if (!ActiveSkill->HitSequence.IsValidIndex(HitIndex))
	{
		UE_LOG(LogRetrieveCombat, Warning,
			TEXT("[PlayerBurstComponent] OnBurstHit(%d) out of range (HitSequence=%d). Owner=%s"),
			HitIndex,
			ActiveSkill->HitSequence.Num(),
			*GetNameSafe(GetOwner()));
		return;
	}

	const FBurstHitInstance& Hit = ActiveSkill->HitSequence[HitIndex];

	switch (ActiveSkill->AttackType)
	{
	case EBurstAttackType::Cleave:       DoCleaveHit(Hit, HitIndex); break;
	case EBurstAttackType::WorldActor:   DoWorldActorHit(Hit, HitIndex); break;
	case EBurstAttackType::Projectile:   DoProjectileHit(Hit, HitIndex); break;
	case EBurstAttackType::Dash:         DoDashHit(Hit, HitIndex); break;
	case EBurstAttackType::AreaOfEffect: DoAoEHit(Hit, HitIndex); break;
	default: break;
	}
}

FVector UPlayerBurstComponent::ResolveSourceLocation(const FBurstHitInstance& Hit) const
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner)) return FVector::ZeroVector;

	switch (Hit.HitSource)
	{
	case EBurstHitSource::Sword:
	{
		UWeaponComponent* WeaponComp = Owner->FindComponentByClass<UWeaponComponent>();
		UMeshComponent* WeaponMesh = IsValid(WeaponComp) ? WeaponComp->GetPrimaryEquippedWeaponMesh() : nullptr;
		const FName SocketName = !Hit.SocketOverride.IsNone()
			? Hit.SocketOverride
			: (IsValid(WeaponComp) && !WeaponComp->GetWeaponDataRef().TraceSocketName.IsNone()
				? WeaponComp->GetWeaponDataRef().TraceSocketName
				: RetrieveWeaponSockets::Weapon_R);
		if (IsValid(WeaponMesh) && WeaponMesh->DoesSocketExist(SocketName))
		{
			return WeaponMesh->GetSocketLocation(SocketName);
		}
		// Fallback: 캐릭터 메쉬 소켓
		if (ACharacter* Character = Cast<ACharacter>(Owner))
		{
			if (USkeletalMeshComponent* CharMesh = Character->GetMesh())
			{
				if (CharMesh->DoesSocketExist(SocketName))
				{
					return CharMesh->GetSocketLocation(SocketName);
				}
			}
		}
		return Owner->GetActorLocation();
	}
	case EBurstHitSource::Shield:
	{
		const FName SocketName = !Hit.SocketOverride.IsNone() ? Hit.SocketOverride : RetrieveWeaponSockets::Shield;
		if (ACharacter* Character = Cast<ACharacter>(Owner))
		{
			if (USkeletalMeshComponent* CharMesh = Character->GetMesh())
			{
				if (CharMesh->DoesSocketExist(SocketName))
				{
					return CharMesh->GetSocketLocation(SocketName);
				}
			}
		}
		return Owner->GetActorLocation();
	}
	case EBurstHitSource::Body:
		return Owner->GetActorLocation();
	case EBurstHitSource::World:
		return SpawnedWorldActor.IsValid() ? SpawnedWorldActor->GetActorLocation() : Owner->GetActorLocation();
	default:
		return Owner->GetActorLocation();
	}
}

void UPlayerBurstComponent::SweepAndApply(const FBurstHitInstance& Hit, const FVector& CurrentOrigin, float Radius, int32 HitIndex)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !ActiveSkill) return;
	if (!Owner->HasAuthority()) return;
	if (!IsValid(ActiveSkill->DamageEffect)) return;

	UWorld* World = Owner->GetWorld();
	if (!IsValid(World)) return;

	const bool bHasPrev = PerHitHasPrevious.IsValidIndex(HitIndex) && PerHitHasPrevious[HitIndex];
	const FVector SweepStart = bHasPrev ? PerHitPreviousOrigin[HitIndex] : CurrentOrigin;

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PlayerBurst_Sweep), false, Owner);

	TArray<FHitResult> HitResults;
	const bool bHit = World->SweepMultiByObjectType(
		HitResults,
		SweepStart,
		CurrentOrigin,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(Radius),
		QueryParams);

	if (bDebugDrawTrace)
	{
		constexpr float DebugLife = -1.0f;
		DrawDebugLine(World, SweepStart, CurrentOrigin, bHit ? FColor::Green : FColor::Red, false, DebugLife, 0, 0.5f);
		DrawDebugSphere(World, CurrentOrigin, Radius, 12, bHit ? FColor::Green : FColor::Red, false, DebugLife);
	}

	if (PerHitPreviousOrigin.IsValidIndex(HitIndex))
	{
		PerHitPreviousOrigin[HitIndex] = CurrentOrigin;
		PerHitHasPrevious[HitIndex] = true;
	}

	if (!bHit) return;

	TSet<TObjectPtr<AActor>>* HitSet = PerHitHitActors.IsValidIndex(HitIndex) ? &PerHitHitActors[HitIndex] : nullptr;

	for (const FHitResult& HitResult : HitResults)
	{
		AActor* Target = HitResult.GetActor();
		if (!IsValid(Target) || Target == Owner) continue;
		if (HitSet && HitSet->Contains(Target)) continue;

		ApplyHitToTarget(Target, Hit, HitResult);

		if (HitSet)
		{
			HitSet->Add(Target);
		}
	}
}

void UPlayerBurstComponent::ReportProjectileHit(AActor* Target, const FBurstHitInstance& Hit, const FHitResult& HitResult)
{
	ApplyHitToTarget(Target, Hit, HitResult);
}

void UPlayerBurstComponent::ApplyHitToTarget(AActor* Target, const FBurstHitInstance& Hit, const FHitResult& HitResult)
{
	if (!IsValid(Target) || !ActiveSkill || !IsValid(ActiveSkill->DamageEffect)) return;

	AActor* Owner = GetOwner();
	UAbilitySystemComponent* SourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner);
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
	if (!IsValid(SourceASC) || !IsValid(TargetASC)) return;

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddInstigator(Owner, Owner);
	Context.AddSourceObject(this);
	Context.AddHitResult(HitResult, /*bReset=*/true);

	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(ActiveSkill->DamageEffect, 1.f, Context);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid()) return;

	SpecHandle.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Damage_Mul, Hit.DamageMultiplier);

	SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
}

void UPlayerBurstComponent::DoCleaveHit(const FBurstHitInstance& Hit, int32 HitIndex)
{
	SweepAndApply(Hit, ResolveSourceLocation(Hit), CleaveRadius, HitIndex);
}

void UPlayerBurstComponent::DoProjectileHit(const FBurstHitInstance& Hit, int32 HitIndex)
{
	// 중복 스폰 방지: NotifyTick 으로 매 프레임 호출되더라도 같은 HitIndex 는 1회만 발사
	if (PerHitProjectileSpawned.IsValidIndex(HitIndex) && PerHitProjectileSpawned[HitIndex])
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !ActiveSkill) return;
	if (!Owner->HasAuthority()) return;
	if (!IsValid(ActiveSkill->ProjectileClass)) return;

	UWorld* World = Owner->GetWorld();
	if (!IsValid(World)) return;

	if (!ActiveSkill->ProjectileClass->IsChildOf(ABurstProjectile::StaticClass()))
	{
		UE_LOG(LogRetrieveCombat, Warning,
			TEXT("[PlayerBurstComponent] DoProjectileHit: ProjectileClass %s is not ABurstProjectile subclass."),
			*GetNameSafe(ActiveSkill->ProjectileClass));
		return;
	}

	const FVector SpawnLocation = ResolveSourceLocation(Hit);
	// 검기 기울임: 진행 방향은 정면 고정(Vector()는 Roll 무시), 자세만 진행축 기준 회전.
	FRotator SpawnRotation = Owner->GetActorRotation();
	SpawnRotation.Roll += Hit.LaunchRollAngle;
	const FVector LaunchDirection = SpawnRotation.Vector();
	const FTransform SpawnTransform(SpawnRotation, SpawnLocation);

	APawn* InstigatorPawn = Cast<APawn>(Owner);

	ABurstProjectile* Projectile = World->SpawnActorDeferred<ABurstProjectile>(
		ActiveSkill->ProjectileClass,
		SpawnTransform,
		Owner,
		InstigatorPawn,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);

	if (!IsValid(Projectile))
	{
		UE_LOG(LogRetrieveCombat, Warning,
			TEXT("[PlayerBurstComponent] DoProjectileHit: SpawnActorDeferred failed. Owner=%s, HitIndex=%d"),
			*GetNameSafe(Owner), HitIndex);
		return;
	}

	Projectile->Initialize(Hit, Owner);
	Projectile->FinishSpawning(SpawnTransform);
	Projectile->Launch(LaunchDirection, DefaultProjectileSpeed);

	// 스폰 성공 → 같은 HitIndex 재호출 차단
	if (PerHitProjectileSpawned.IsValidIndex(HitIndex))
	{
		PerHitProjectileSpawned[HitIndex] = true;
	}

	UE_LOG(LogRetrieveCombat, Log,
		TEXT("[PlayerBurstComponent] DoProjectileHit. HitIndex=%d, Class=%s"),
		HitIndex,
		*GetNameSafe(ActiveSkill->ProjectileClass));
}

void UPlayerBurstComponent::DoWorldActorHit(const FBurstHitInstance& Hit, int32 HitIndex)
{
	// 첫 호출 시 World 액터 1개 스폰. 이후 호출은 같은 액터의 영역에서 sweep만 수행.
	if (HitIndex == 0 && !SpawnedWorldActor.IsValid())
	{
		AActor* Owner = GetOwner();
		if (IsValid(Owner) && Owner->HasAuthority() && ActiveSkill && IsValid(ActiveSkill->WorldSpawnActorClass))
		{
			UWorld* World = Owner->GetWorld();
			if (IsValid(World))
			{
				const FVector SpawnLocation = Owner->GetActorLocation()
					+ Owner->GetActorForwardVector() * ActiveSkill->WorldSpawnDistance;
				const FRotator SpawnRotation = Owner->GetActorRotation();
				const FTransform SpawnTransform(SpawnRotation, SpawnLocation);

				FActorSpawnParameters SpawnParams;
				SpawnParams.Owner = Owner;
				SpawnParams.Instigator = Cast<APawn>(Owner);
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

				AActor* Spawned = World->SpawnActor<AActor>(
					ActiveSkill->WorldSpawnActorClass, SpawnTransform, SpawnParams);

				if (IsValid(Spawned))
				{
					SpawnedWorldActor = Spawned;
					UE_LOG(LogRetrieveCombat, Log,
						TEXT("[PlayerBurstComponent] DoWorldActorHit. Spawned %s at %s"),
						*GetNameSafe(Spawned), *SpawnLocation.ToCompactString());
				}
				else
				{
					UE_LOG(LogRetrieveCombat, Warning,
						TEXT("[PlayerBurstComponent] DoWorldActorHit. SpawnActor failed. Class=%s"),
						*GetNameSafe(ActiveSkill->WorldSpawnActorClass));
				}
			}
		}
	}

	// 매 HitIndex 마다 sweep + 데미지 적용
	SweepAndApply(Hit, ResolveSourceLocation(Hit), WorldActorRadius, HitIndex);
}

void UPlayerBurstComponent::DoDashHit(const FBurstHitInstance& Hit, int32 HitIndex)
{
	SweepAndApply(Hit, ResolveSourceLocation(Hit), DashRadius, HitIndex);
}

void UPlayerBurstComponent::DoAoEHit(const FBurstHitInstance& Hit, int32 HitIndex)
{
	const float Radius = ActiveSkill ? ActiveSkill->AoeRadius : 0.f;
	SweepAndApply(Hit, ResolveSourceLocation(Hit), Radius, HitIndex);
}
