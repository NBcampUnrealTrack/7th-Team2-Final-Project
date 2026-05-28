#include "Components/PlayerBurstComponent.h"

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
	for (int32 Index = 0; Index < HitCount; ++Index)
	{
		PerHitHitActors[Index].Reset();
		PerHitPreviousOrigin[Index] = FVector::ZeroVector;
		PerHitHasPrevious[Index] = false;
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
	case EBurstAttackType::Cleave:         DoCleaveHit(Hit, HitIndex); break;
	case EBurstAttackType::GroundEruption: DoGroundEruptionHit(Hit, HitIndex); break;
	case EBurstAttackType::Projectile:     DoProjectileHit(Hit, HitIndex); break;
	case EBurstAttackType::Dash:           DoDashHit(Hit, HitIndex); break;
	case EBurstAttackType::AreaOfEffect:   DoAoEHit(Hit, HitIndex); break;
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
	// TODO: ActiveSkill->ProjectileClass를 SpawnActor하고, Projectile OnHit 콜백에서
	//       이 컴포넌트의 ApplyHitToTarget을 호출하도록 연결.
}

void UPlayerBurstComponent::DoGroundEruptionHit(const FBurstHitInstance& Hit, int32 HitIndex)
{
	SweepAndApply(Hit, ResolveSourceLocation(Hit), GroundEruptionRadius, HitIndex);
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
