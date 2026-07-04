#include "Components/Player/PlayerBurstComponent.h"

#include "AbilitySystem/Player/BurstProjectile.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/RetrieveWeaponSockets.h"
#include "Combat/RetrieveKnockbackLibrary.h"
#include "Components/MeshComponent.h"
#include "Components/Player/WeaponComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "Logging/RetrieveLogChannels.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

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

	// 버스트 행 → 공용 실행 spec 으로 복사 후 위임. (FSkillCombination 미수술; 마지막 개선 단계서 완전 이동 예정)
	FAttackExecutionSpec Spec;
	Spec.AttackType           = Row->AttackType;
	Spec.DamageEffect         = Row->DamageEffect;
	Spec.ProjectileClass      = Row->ProjectileClass;
	Spec.WorldSpawnActorClass = Row->WorldSpawnActorClass;
	Spec.WorldSpawnDistance   = Row->WorldSpawnDistance;
	Spec.bWorldSnapToGround     = Row->bWorldSnapToGround;
	Spec.WorldSpawnHeightOffset = Row->WorldSpawnHeightOffset;
	Spec.WorldActorRadius       = Row->WorldActorRadius;
	Spec.AoeRadius            = Row->AoeRadius;
	Spec.ConeRadius           = Row->ConeRadius;
	Spec.ConeHalfAngleDeg     = Row->ConeHalfAngleDeg;
	Spec.ContinuousDamageInterval          = Row->ContinuousDamageInterval;
	Spec.bUseContinuousKnockback           = Row->bUseContinuousKnockback;
	Spec.ContinuousKnockback               = Row->ContinuousKnockback;
	Spec.bExcludeBossFromContinuousKnockback = Row->bExcludeBossFromContinuousKnockback;
	Spec.HitSequence          = Row->HitSequence;

	BeginAttackExecution(Spec);
}

void UPlayerBurstComponent::EndBurstSkill()
{
	UE_LOG(LogRetrieveCombat, Log,
		TEXT("[PlayerBurstComponent] EndBurstSkill. Owner=%s"),
		*GetNameSafe(GetOwner()));

	bHasActiveSpec = false;
	ActiveSpec = FAttackExecutionSpec();
	SpawnedWorldActor.Reset();
	PerHitHitActors.Reset();
	PerHitPreviousPoints.Reset();
	PerHitHasPrevious.Reset();
	PerHitProjectileSpawned.Reset();
	PerHitDashLaunched.Reset();
	PerHitLastHitTime.Reset();
}

void UPlayerBurstComponent::BeginAttackExecution(const FAttackExecutionSpec& Spec)
{
	ActiveSpec = Spec;
	bHasActiveSpec = true;
	SpawnedWorldActor.Reset();

	const int32 HitCount = ActiveSpec.HitSequence.Num();
	PerHitHitActors.SetNum(HitCount);
	PerHitPreviousPoints.SetNum(HitCount);
	PerHitHasPrevious.SetNum(HitCount);
	PerHitProjectileSpawned.SetNum(HitCount);
	PerHitDashLaunched.SetNum(HitCount);
	PerHitLastHitTime.SetNum(HitCount);
	for (int32 Index = 0; Index < HitCount; ++Index)
	{
		PerHitHitActors[Index].Reset();
		PerHitPreviousPoints[Index].Reset();
		PerHitHasPrevious[Index] = false;
		PerHitProjectileSpawned[Index] = false;
		PerHitDashLaunched[Index] = false;
		PerHitLastHitTime[Index].Reset();
	}

	UE_LOG(LogRetrieveCombat, Log,
		TEXT("[PlayerBurstComponent] BeginAttackExecution. Owner=%s, HitCount=%d"),
		*GetNameSafe(GetOwner()), HitCount);
}

void UPlayerBurstComponent::OnBurstHit(int32 HitIndex)
{
	if (!bHasActiveSpec)
	{
		UE_LOG(LogRetrieveCombat, Warning,
			TEXT("[PlayerBurstComponent] OnBurstHit(%d) called with no active skill. Owner=%s"),
			HitIndex,
			*GetNameSafe(GetOwner()));
		return;
	}

	if (!ActiveSpec.HitSequence.IsValidIndex(HitIndex))
	{
		UE_LOG(LogRetrieveCombat, Warning,
			TEXT("[PlayerBurstComponent] OnBurstHit(%d) out of range (HitSequence=%d). Owner=%s"),
			HitIndex,
			ActiveSpec.HitSequence.Num(),
			*GetNameSafe(GetOwner()));
		return;
	}

	const FBurstHitInstance& Hit = ActiveSpec.HitSequence[HitIndex];

	switch (ActiveSpec.AttackType)
	{
	case EAttackExecutionType::Cleave:       DoCleaveHit(Hit, HitIndex); break;
	case EAttackExecutionType::WorldActor:   DoWorldActorHit(Hit, HitIndex); break;
	case EAttackExecutionType::Projectile:   DoProjectileHit(Hit, HitIndex); break;
	case EAttackExecutionType::Dash:         DoDashHit(Hit, HitIndex); break;
	case EAttackExecutionType::AreaOfEffect: DoAoEHit(Hit, HitIndex); break;
	case EAttackExecutionType::Cone:         DoConeHit(Hit, HitIndex); break;
	case EAttackExecutionType::AreaContinuous: DoAreaContinuousHit(Hit, HitIndex); break;
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
	if (!IsValid(Owner) || !bHasActiveSpec) return;
	if (!Owner->HasAuthority()) return;
	if (!IsValid(ActiveSpec.DamageEffect)) return;

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
	if (!IsValid(Target) || !bHasActiveSpec || !IsValid(ActiveSpec.DamageEffect)) return;

	AActor* Owner = GetOwner();
	UAbilitySystemComponent* SourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner);
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
	if (!IsValid(SourceASC) || !IsValid(TargetASC)) return;

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddInstigator(Owner, Owner);
	Context.AddSourceObject(this);
	Context.AddHitResult(HitResult, /*bReset=*/true);

	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(ActiveSpec.DamageEffect, 1.f, Context);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid()) return;

	SpecHandle.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Damage_Mul, Hit.DamageMultiplier);

	const FGameplayTag HitSuccessTag = Hit.HitSuccessFeedbackTag.IsValid()
	? Hit.HitSuccessFeedbackTag
	: RetrieveGameplayTags::GameplayEvent_Attack_HitSuccess_Burst;

	const FGameplayTag TargetHitTag = Hit.TargetHitFeedbackTag.IsValid()
	? Hit.TargetHitFeedbackTag
	: RetrieveGameplayTags::GameplayEvent_Hit_Heavy;

	SpecHandle.Data->AddDynamicAssetTag(HitSuccessTag);
	SpecHandle.Data->AddDynamicAssetTag(TargetHitTag);

	if (Hit.KnockbackStrength > 0.f)
	{
		SpecHandle.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Knockback_Strength, Hit.KnockbackStrength);
		SpecHandle.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Knockback_UpwardStrength, Hit.KnockbackUpwardStrength);
	}
	
	AddCombatTagsToDamageSpec(*SpecHandle.Data.Get(), ActiveSpec.ElementTag, ActiveSpec.AttackTypeTag, ActiveSpec.AttackPropertyTag);
	if (ActiveSpec.HitEventTag.IsValid())
	{
		SpecHandle.Data->AddDynamicAssetTag(ActiveSpec.HitEventTag);
	}

	SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

	// ── 적중 VFX/사운드 (FBurstHitInstance 데이터 기반) ──
	PlayHitFeedback(Hit, HitResult, Target);

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

void UPlayerBurstComponent::PlayHitFeedback(const FBurstHitInstance& Hit, const FHitResult& HitResult, AActor* Target) const
{
	UWorld* World = GetWorld();
	if (!IsValid(World)) return;

	const FVector FxLocation = HitResult.ImpactPoint.IsNearlyZero()
		? (IsValid(Target) ? Target->GetActorLocation() : FVector::ZeroVector)
		: HitResult.ImpactPoint;

	if (UNiagaraSystem* VFX = Hit.HitVFX.LoadSynchronous())
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, VFX, FxLocation, HitResult.ImpactNormal.Rotation());
	}

	if (USoundBase* SFX = Hit.HitSound.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(World, SFX, FxLocation);
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
	if (!IsValid(Owner) || !bHasActiveSpec) return;
	if (!Owner->HasAuthority()) return;
	if (!IsValid(ActiveSpec.ProjectileClass)) return;

	UWorld* World = Owner->GetWorld();
	if (!IsValid(World)) return;

	if (!ActiveSpec.ProjectileClass->IsChildOf(ABurstProjectile::StaticClass()))
	{
		UE_LOG(LogRetrieveCombat, Warning,
			TEXT("[PlayerBurstComponent] DoProjectileHit: ProjectileClass %s is not ABurstProjectile subclass."),
			*GetNameSafe(ActiveSpec.ProjectileClass));
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
		ActiveSpec.ProjectileClass,
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
		*GetNameSafe(ActiveSpec.ProjectileClass));
}

void UPlayerBurstComponent::DoWorldActorHit(const FBurstHitInstance& Hit, int32 HitIndex)
{
	// 첫 호출 시 World 액터 1개 스폰. 이후 호출은 같은 액터의 영역에서 sweep만 수행.
	if (HitIndex == 0 && !SpawnedWorldActor.IsValid())
	{
		AActor* Owner = GetOwner();
		if (IsValid(Owner) && Owner->HasAuthority() && bHasActiveSpec && IsValid(ActiveSpec.WorldSpawnActorClass))
		{
			UWorld* World = Owner->GetWorld();
			if (IsValid(World))
			{
				FVector SpawnLocation = Owner->GetActorLocation()
					+ Owner->GetActorForwardVector() * ActiveSpec.WorldSpawnDistance;

				// 전방 지점 아래 지면을 찾아 착지 지점을 정렬한 뒤, 그 위 HeightOffset 높이에서 스폰
				if (ActiveSpec.bWorldSnapToGround)
				{
					FHitResult GroundHit;
					const FVector TraceStart = SpawnLocation + FVector(0.f, 0.f, 500.f);
					const FVector TraceEnd = SpawnLocation - FVector(0.f, 0.f, 5000.f);
					FCollisionQueryParams GroundParams(SCENE_QUERY_STAT(BurstWorldActorGroundSnap), false, Owner);
					if (World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, GroundParams))
					{
						SpawnLocation.Z = GroundHit.ImpactPoint.Z;
					}
				}
				SpawnLocation.Z += ActiveSpec.WorldSpawnHeightOffset;

				const FRotator SpawnRotation = Owner->GetActorRotation();
				const FTransform SpawnTransform(SpawnRotation, SpawnLocation);

				FActorSpawnParameters SpawnParams;
				SpawnParams.Owner = Owner;
				SpawnParams.Instigator = Cast<APawn>(Owner);
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

				AActor* Spawned = World->SpawnActor<AActor>(
					ActiveSpec.WorldSpawnActorClass, SpawnTransform, SpawnParams);

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
						*GetNameSafe(ActiveSpec.WorldSpawnActorClass));
				}
			}
		}
	}

	// 반경: 데이터(WorldActorRadius) 우선, 0이면 컴포넌트 기본
	const float WActorR = (bHasActiveSpec && ActiveSpec.WorldActorRadius > 0.f) ? ActiveSpec.WorldActorRadius : WorldActorRadius;
	SweepAndApply(Hit, ResolveSourceLocation(Hit), WActorR, HitIndex);
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

		const bool bPerHitDash = Hit.bOverrideDashMotion;
		const float DashDistance = bPerHitDash ? Hit.DashForwardDistance : (bHasActiveSpec ? ActiveSpec.DashDistance : 0.f);
		const float DashLaunchDuration = bPerHitDash ? Hit.DashLaunchDuration : (bHasActiveSpec ? ActiveSpec.DashLaunchDuration : 0.f);
		const float DashUpwardSpeed = bPerHitDash ? Hit.DashUpwardSpeed : (bHasActiveSpec ? ActiveSpec.DashUpwardSpeed : 0.f);
		const bool bHasUpward = !FMath::IsNearlyZero(DashUpwardSpeed);
		const bool bHasForward = !FMath::IsNearlyZero(DashDistance);
		if (IsValid(Character) && DashLaunchDuration > 0.f && (bHasForward || bHasUpward))
		{
			FVector Forward = Character->GetActorForwardVector();
			Forward.Z = 0.f;
			Forward = Forward.GetSafeNormal();

			const float HorizontalSpeed = DashDistance / DashLaunchDuration;
			FVector LaunchVelocity = Forward * HorizontalSpeed;
			LaunchVelocity.Z += DashUpwardSpeed;

			if (!LaunchVelocity.IsNearlyZero())
			{
				URetrieveKnockbackLibrary::LaunchSelf(Character, LaunchVelocity.GetSafeNormal(), LaunchVelocity.Size(), /*bOverrideXY=*/true, /*bOverrideZ=*/bHasUpward);

				UE_LOG(LogRetrieveCombat, Log,
					TEXT("[PlayerBurstComponent] DoDashHit. HitIndex=%d, Dash=%.1f, Up=%.1f, Speed=%.1f"),
					HitIndex, DashDistance, DashUpwardSpeed, LaunchVelocity.Size());
			}
		}
	}

	SweepAndApply(Hit, ResolveSourceLocation(Hit), DashRadius, HitIndex);
}

void UPlayerBurstComponent::DoAoEHit(const FBurstHitInstance& Hit, int32 HitIndex)
{
	const float Radius = bHasActiveSpec ? ActiveSpec.AoeRadius : 0.f;
	SweepAndApply(Hit, ResolveSourceLocation(Hit), Radius, HitIndex);
}

void UPlayerBurstComponent::DoConeHit(const FBurstHitInstance& Hit, int32 HitIndex)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority() || !bHasActiveSpec || !IsValid(ActiveSpec.DamageEffect)) return;

	const float Radius = ActiveSpec.ConeRadius;
	if (Radius <= 0.f) return;

	UWorld* World = Owner->GetWorld();
	if (!IsValid(World)) return;

	const FVector Origin = Owner->GetActorLocation();
	const FVector Forward = Owner->GetActorForwardVector().GetSafeNormal2D();
	const float CosHalf = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(ActiveSpec.ConeHalfAngleDeg, 0.f, 180.f)));

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel1);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PlayerBurst_ConeHit), false, Owner);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, ObjectQueryParams, FCollisionShape::MakeSphere(Radius), QueryParams);

	if (bDebugDrawTrace)
	{
		const FVector EdgeL = Forward.RotateAngleAxis(ActiveSpec.ConeHalfAngleDeg, FVector::UpVector);
		const FVector EdgeR = Forward.RotateAngleAxis(-ActiveSpec.ConeHalfAngleDeg, FVector::UpVector);
		DrawDebugSphere(World, Origin, Radius, 16, FColor::Cyan, false, 1.5f);
		DrawDebugLine(World, Origin, Origin + Forward * Radius, FColor::Green, false, 1.5f, 0, 2.f);
		DrawDebugLine(World, Origin, Origin + EdgeL * Radius, FColor::Yellow, false, 1.5f, 0, 2.f);
		DrawDebugLine(World, Origin, Origin + EdgeR * Radius, FColor::Yellow, false, 1.5f, 0, 2.f);
		UE_LOG(LogRetrieveCombat, Log, TEXT("[Burst] DoConeHit HitIndex=%d Radius=%.0f Half=%.0f Overlaps=%d DmgGE=%s"),
			HitIndex, Radius, ActiveSpec.ConeHalfAngleDeg, Overlaps.Num(), *GetNameSafe(ActiveSpec.DamageEffect));
	}
	if (Overlaps.IsEmpty()) return;

	// HitIndex별 1회 히트(중복 방지)
	TSet<TObjectPtr<AActor>>* HitSet = PerHitHitActors.IsValidIndex(HitIndex) ? &PerHitHitActors[HitIndex] : nullptr;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (!IsValid(Target) || Target == Owner) continue;
		if (HitSet && HitSet->Contains(Target)) continue;

		// 전방 부채꼴(수평) 각도 안만
		const FVector ToTarget = (Target->GetActorLocation() - Origin).GetSafeNormal2D();
		if (!Forward.IsNearlyZero() && FVector::DotProduct(Forward, ToTarget) < CosHalf) continue;

		FHitResult HitResult;
		HitResult.ImpactPoint = Target->GetActorLocation();
		ApplyHitToTarget(Target, Hit, HitResult);

		if (HitSet)
		{
			HitSet->Add(Target);
		}
	}
}

void UPlayerBurstComponent::DoAreaContinuousHit(const FBurstHitInstance& Hit, int32 HitIndex)
{
	// 지속 범위(소용돌이): 노티 구간 동안 매 프레임 호출.
	//  - 데미지: 적별 ContinuousDamageInterval 간격 재적용(첫 진입 즉시 1틱).
	//  - 넉백: 매 프레임 중심→바깥 방사. GE 방향성 넉백은 억제하고 방사로만 민다.
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority() || !bHasActiveSpec || !IsValid(ActiveSpec.DamageEffect)) return;

	const float Radius = ActiveSpec.AoeRadius;
	if (Radius <= 0.f) return;

	UWorld* World = Owner->GetWorld();
	if (!IsValid(World)) return;

	const FVector Center = Owner->GetActorLocation();

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel1);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PlayerBurst_AreaContinuous), false, Owner);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, ObjectQueryParams, FCollisionShape::MakeSphere(Radius), QueryParams);

	if (bDebugDrawTrace)
	{
		DrawDebugSphere(World, Center, Radius, 24, Overlaps.Num() > 0 ? FColor::Cyan : FColor::Blue, false, -1.f);
	}
	if (Overlaps.IsEmpty()) return;

	const double Now = World->GetTimeSeconds();
	const float Interval = FMath::Max(0.f, ActiveSpec.ContinuousDamageInterval);
	TMap<TWeakObjectPtr<AActor>, double>* TimeMap = PerHitLastHitTime.IsValidIndex(HitIndex) ? &PerHitLastHitTime[HitIndex] : nullptr;

	// 방사 넉백을 단일 소스로 두기 위해 데미지 틱에서는 GE 넉백을 끈다(이중 넉백 방지).
	FBurstHitInstance DamageHit = Hit;
	DamageHit.KnockbackStrength = 0.f;
	DamageHit.KnockbackUpwardStrength = 0.f;

	TArray<ACharacter*> KnockbackTargets;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (!IsValid(Target) || Target == Owner) continue;

		// 적별 간격 데미지: 첫 진입 즉시 1틱, 이후 Interval 경과 시마다 재적용.
		bool bApplyDamage = true;
		if (TimeMap && Interval > 0.f)
		{
			if (const double* Last = TimeMap->Find(Target))
			{
				bApplyDamage = (Now - *Last) >= Interval;
			}
		}
		if (bApplyDamage)
		{
			FHitResult HitResult;
			HitResult.ImpactPoint = Target->GetActorLocation();
			ApplyHitToTarget(Target, DamageHit, HitResult);
			if (TimeMap)
			{
				TimeMap->Add(Target, Now);
			}
		}

		// 방사 넉백 대상 수집(보스 제외 옵션). 데미지 틱과 무관하게 매 프레임 지속 밀어내기.
		if (ActiveSpec.bUseContinuousKnockback)
		{
			const UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
			const bool bIsBoss = TargetASC && TargetASC->HasMatchingGameplayTag(RetrieveGameplayTags::Monster_Type_Boss);
			if (!(ActiveSpec.bExcludeBossFromContinuousKnockback && bIsBoss))
			{
				if (ACharacter* HitChar = Cast<ACharacter>(Target))
				{
					KnockbackTargets.Add(HitChar);
				}
			}
		}
	}

	if (ActiveSpec.bUseContinuousKnockback && KnockbackTargets.Num() > 0)
	{
		URetrieveKnockbackLibrary::ApplyRadialKnockbackToTargets(Center, Radius, KnockbackTargets, ActiveSpec.ContinuousKnockback);
	}
}

void UPlayerBurstComponent::ApplyLandingImpact(const FVector& Center, float Radius, float DamageMultiplier, bool bUseKnockback, const FRetrieveKnockbackParams& Knockback, bool bExcludeBoss)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority() || !bHasActiveSpec || !IsValid(ActiveSpec.DamageEffect)) return;
	if (Radius <= 0.f) return;

	UWorld* World = Owner->GetWorld();
	if (!IsValid(World)) return;

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel1);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PlayerBurst_LandingImpact), false, Owner);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, ObjectQueryParams, FCollisionShape::MakeSphere(Radius), QueryParams);

	if (bDebugDrawTrace)
	{
		DrawDebugSphere(World, Center, Radius, 16, Overlaps.Num() > 0 ? FColor::Green : FColor::Orange, false, 2.f);
	}
	if (Overlaps.IsEmpty()) return;

	// 착지 슬램 합성 타격(데미지/태그/VFX는 ApplyHitToTarget이 처리)
	FBurstHitInstance LandingHit;
	LandingHit.DamageMultiplier = DamageMultiplier;
	LandingHit.HitSource = EBurstHitSource::Body;

	TSet<TObjectPtr<AActor>> HitOnce;
	TArray<ACharacter*> KnockbackTargets;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (!IsValid(Target) || Target == Owner || HitOnce.Contains(Target)) continue;
		HitOnce.Add(Target);

		FHitResult HitResult;
		HitResult.ImpactPoint = Target->GetActorLocation();
		ApplyHitToTarget(Target, LandingHit, HitResult);

		if (bUseKnockback)
		{
			// 보스는 넉백 제외(옵션)
			const UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
			const bool bIsBoss = TargetASC && TargetASC->HasMatchingGameplayTag(RetrieveGameplayTags::Monster_Type_Boss);
			if (!(bExcludeBoss && bIsBoss))
			{
				if (ACharacter* HitChar = Cast<ACharacter>(Target))
				{
					KnockbackTargets.Add(HitChar);
				}
			}
		}
	}

	if (bUseKnockback && KnockbackTargets.Num() > 0)
	{
		URetrieveKnockbackLibrary::ApplyRadialKnockbackToTargets(Center, Radius, KnockbackTargets, Knockback);
	}
}
