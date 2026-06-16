#include "Components/Player/PlayerBurstComponent.h"

#include "AbilitySystem/Player/BurstProjectile.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/RetrieveWeaponSockets.h"
#include "Components/MeshComponent.h"
#include "Components/Player/WeaponComponent.h"
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
	PerHitPreviousPoints.SetNum(HitCount);
	PerHitHasPrevious.SetNum(HitCount);
	PerHitProjectileSpawned.SetNum(HitCount);
	PerHitDashLaunched.SetNum(HitCount);
	for (int32 Index = 0; Index < HitCount; ++Index)
	{
		PerHitHitActors[Index].Reset();
		PerHitPreviousPoints[Index].Reset();
		PerHitHasPrevious[Index] = false;
		PerHitProjectileSpawned[Index] = false;
		PerHitDashLaunched[Index] = false;
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
	PerHitPreviousPoints.Reset();
	PerHitHasPrevious.Reset();
	PerHitProjectileSpawned.Reset();
	PerHitDashLaunched.Reset();
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

void UPlayerBurstComponent::BuildSwordTracePoints(const FBurstHitInstance& Hit, TArray<FVector>& OutPoints) const
{
	OutPoints.Reset();

	AActor* Owner = GetOwner();
	UWeaponComponent* WeaponComp = IsValid(Owner) ? Owner->FindComponentByClass<UWeaponComponent>() : nullptr;
	if (IsValid(WeaponComp))
	{
		const FRetrieveWeaponDataRow& WeaponData = WeaponComp->GetWeaponDataRef();
		const FName StartSocket = WeaponData.TraceStartSocketName;
		const FName EndSocket = WeaponData.TraceEndSocketName;

		UMeshComponent* TraceMesh = WeaponComp->GetWeaponMeshForTrace(StartSocket, EndSocket);

		if (IsValid(TraceMesh) && !StartSocket.IsNone() && !EndSocket.IsNone()
			&& TraceMesh->DoesSocketExist(StartSocket) && TraceMesh->DoesSocketExist(EndSocket))
		{
			const FVector StartLoc = TraceMesh->GetSocketLocation(StartSocket);
			const FVector EndLoc = TraceMesh->GetSocketLocation(EndSocket);
			const int32 SegmentCount = FMath::Max(2, WeaponData.TraceSegmentCount);

			OutPoints.Reserve(SegmentCount);
			for (int32 i = 0; i < SegmentCount; ++i)
			{
				const float Alpha = static_cast<float>(i) / static_cast<float>(SegmentCount - 1);
				OutPoints.Add(FMath::Lerp(StartLoc, EndLoc, Alpha));
			}
			return;
		}
	}

	// 폴백: 양 끝 소켓이 없으면 기존 단일 소켓(SocketOverride/TraceSocketName/Weapon_R) 해석을 그대로 사용.
	OutPoints.Add(ResolveSourceLocation(Hit));
}

void UPlayerBurstComponent::SweepAndApply(const FBurstHitInstance& Hit, const FVector& CurrentOrigin, float Radius, int32 HitIndex)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !ActiveSkill) return;
	if (!Owner->HasAuthority()) return;
	if (!IsValid(ActiveSkill->DamageEffect)) return;

	UWorld* World = Owner->GetWorld();
	if (!IsValid(World)) return;

	// 검(Sword) 히트는 GA_Attack과 동일한 멀티포인트 블레이드 트레이스, 그 외 소스는 단일 origin.
	TArray<FVector> CurrentPoints;
	if (Hit.HitSource == EBurstHitSource::Sword)
	{
		BuildSwordTracePoints(Hit, CurrentPoints);
	}
	if (CurrentPoints.IsEmpty())
	{
		CurrentPoints.Add(CurrentOrigin);
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel1);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PlayerBurst_Sweep), false, Owner);
	const FCollisionShape TraceShape = FCollisionShape::MakeSphere(Radius);

	const TArray<FVector>* PrevPoints = PerHitPreviousPoints.IsValidIndex(HitIndex) ? &PerHitPreviousPoints[HitIndex] : nullptr;
	const bool bHasPrev = PerHitHasPrevious.IsValidIndex(HitIndex) && PerHitHasPrevious[HitIndex]
		&& PrevPoints && PrevPoints->Num() == CurrentPoints.Num();

	TArray<FHitResult> AllHits;

	auto SweepSegment = [&](const FVector& SegStart, const FVector& SegEnd)
	{
		TArray<FHitResult> SegmentHits;
		const bool bHit = World->SweepMultiByObjectType(SegmentHits, SegStart, SegEnd, FQuat::Identity, ObjectQueryParams, TraceShape, QueryParams);
		if (bDebugDrawTrace)
		{
			constexpr float DebugLife = -1.0f;
			DrawDebugLine(World, SegStart, SegEnd, bHit ? FColor::Green : FColor::Red, false, DebugLife, 0, 0.5f);
			DrawDebugSphere(World, SegEnd, Radius, 12, bHit ? FColor::Green : FColor::Red, false, DebugLife);
		}
		if (bHit)
		{
			AllHits.Append(MoveTemp(SegmentHits));
		}
	};

	// 현재 프레임 블레이드 길이 방향 커버.
	for (int32 i = 0; i + 1 < CurrentPoints.Num(); ++i)
	{
		SweepSegment(CurrentPoints[i], CurrentPoints[i + 1]);
	}

	// 프레임 간 보간(직전 → 현재) 커버. 포인트 수가 같을 때만.
	if (bHasPrev)
	{
		for (int32 i = 0; i < CurrentPoints.Num(); ++i)
		{
			SweepSegment((*PrevPoints)[i], CurrentPoints[i]);
		}
	}
	else if (CurrentPoints.Num() == 1)
	{
		SweepSegment(CurrentPoints[0], CurrentPoints[0]);
	}

	if (PerHitPreviousPoints.IsValidIndex(HitIndex))
	{
		PerHitPreviousPoints[HitIndex] = CurrentPoints;
		PerHitHasPrevious[HitIndex] = true;
	}

	if (AllHits.IsEmpty()) return;

	TSet<TObjectPtr<AActor>>* HitSet = PerHitHitActors.IsValidIndex(HitIndex) ? &PerHitHitActors[HitIndex] : nullptr;

	for (const FHitResult& HitResult : AllHits)
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

	// ── 상태 부여 + 원소 반응 ──
	for (const TSubclassOf<UGameplayEffect>& StatusGE : Hit.StatusEffects)
	{
		if (!IsValid(StatusGE)) continue;

		// 상태 부여 "전"에 반응 검사 (기존 상태 + 새로 들어오는 상태)
		TryElementReaction(SourceASC, TargetASC, StatusGE);

		// 들어온 상태는 반응 여부와 무관하게 적용 (명세: 새 상태는 남김)
		FGameplayEffectSpecHandle StatusSpec = SourceASC->MakeOutgoingSpec(StatusGE, 1.f, Context);
		if (StatusSpec.IsValid() && StatusSpec.Data.IsValid())
		{
			SourceASC->ApplyGameplayEffectSpecToTarget(*StatusSpec.Data.Get(), TargetASC);
		}
	}
}

void UPlayerBurstComponent::TryElementReaction(UAbilitySystemComponent* SourceASC,
	UAbilitySystemComponent* TargetASC, const TSubclassOf<UGameplayEffect>& IncomingStatusGE)
{
	if (!ReactionTable || !IsValid(SourceASC) || !IsValid(TargetASC) || !IsValid(IncomingStatusGE)) return;

	static const FString Ctx(TEXT("PlayerBurst::TryElementReaction"));
	TArray<FElementReactionRow*> Rows;
	ReactionTable->GetAllRows<FElementReactionRow>(Ctx, Rows);

	for (const FElementReactionRow* Row : Rows)
	{
		if (!Row || Row->IncomingStatusEffect != IncomingStatusGE) continue;
		if (!TargetASC->HasMatchingGameplayTag(Row->RequiredExistingTag)) continue;

		// 반응 효과 적용 (추가 데미지 / 이속 디버프 등)
		if (IsValid(Row->ReactionEffect))
		{
			FGameplayEffectContextHandle ReactionCtx = SourceASC->MakeEffectContext();
			ReactionCtx.AddInstigator(GetOwner(), GetOwner());
			ReactionCtx.AddSourceObject(this);

			FGameplayEffectSpecHandle RSpec = SourceASC->MakeOutgoingSpec(Row->ReactionEffect, 1.f, ReactionCtx);
			if (RSpec.IsValid() && RSpec.Data.IsValid())
			{
				SourceASC->ApplyGameplayEffectSpecToTarget(*RSpec.Data.Get(), TargetASC);
			}
		}

		// 기존 상태 제거
		if (Row->RemoveStatusTag.IsValid())
		{
			TargetASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(Row->RemoveStatusTag));
		}

		UE_LOG(LogRetrieveCombat, Log, TEXT("[PlayerBurstComponent] Reaction: incoming=%s removed=%s"),
			*GetNameSafe(IncomingStatusGE), *Row->RemoveStatusTag.ToString());

		break; // 한 상태당 한 반응
	}
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
	// 중복 발사 방지: NotifyTick 으로 매 프레임 호출되더라도 같은 HitIndex 는 1회만 발사
	if (!PerHitDashLaunched.IsValidIndex(HitIndex) || !PerHitDashLaunched[HitIndex])
	{
		if (PerHitDashLaunched.IsValidIndex(HitIndex))
		{
			PerHitDashLaunched[HitIndex] = true;
		}

		ACharacter* Character = Cast<ACharacter>(GetOwner());
		const float DashDistance = ActiveSkill ? ActiveSkill->DashDistance : 0.f;
		const float DashLaunchDuration = ActiveSkill ? ActiveSkill->DashLaunchDuration : 0.f;
		if (IsValid(Character) && DashDistance > 0.f && DashLaunchDuration > 0.f)
		{
			FVector Forward = Character->GetActorForwardVector();
			Forward.Z = 0.f;
			Forward = Forward.GetSafeNormal();
			if (!Forward.IsNearlyZero())
			{
				const float LaunchSpeed = DashDistance / DashLaunchDuration;
				Character->LaunchCharacter(Forward * LaunchSpeed, /*bXYOverride=*/true, /*bZOverride=*/false);

				UE_LOG(LogRetrieveCombat, Log,
					TEXT("[PlayerBurstComponent] DoDashHit. HitIndex=%d, DashDistance=%.1f, LaunchSpeed=%.1f"),
					HitIndex, DashDistance, LaunchSpeed);
			}
		}
	}

	SweepAndApply(Hit, ResolveSourceLocation(Hit), DashRadius, HitIndex);
}

void UPlayerBurstComponent::DoAoEHit(const FBurstHitInstance& Hit, int32 HitIndex)
{
	const float Radius = ActiveSkill ? ActiveSkill->AoeRadius : 0.f;
	SweepAndApply(Hit, ResolveSourceLocation(Hit), Radius, HitIndex);
}
