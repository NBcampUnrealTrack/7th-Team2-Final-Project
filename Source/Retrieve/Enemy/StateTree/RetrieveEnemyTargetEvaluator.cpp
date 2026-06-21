#include "Enemy/StateTree/RetrieveEnemyTargetEvaluator.h"

#include "StateTreeLinker.h"
#include "StateTreeExecutionContext.h"
#include "AIController.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense.h"
#include "Perception/AISense_Damage.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Messaging/GameplayMessages/RetrieveGameplayMessageTypes.h"
#include "Character/RetrieveEnemyCharacter.h"
#include "NavigationSystem.h"
#include "Components/Combat/RetrieveHealthComponent.h"
#include "Enemy/EncirclementSubsystem.h"
#include "Components/Enemy/EnemyCombatComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Logging/RetrieveLogChannels.h"

namespace
{
	bool IsDeadOrDyingActor(AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return true;
		}

		if (URetrieveHealthComponent* HealthComp = Actor->GetComponentByClass<URetrieveHealthComponent>())
		{
			return HealthComp->IsDeadOrDying();
		}

		return false;
	}

	bool WasDamageSensed(UAIPerceptionComponent* PerceptionComp, AActor* Actor)
	{
		if (!PerceptionComp || !IsValid(Actor))
		{
			return false;
		}

		FActorPerceptionBlueprintInfo PerceptionInfo;
		if (!PerceptionComp->GetActorsPerception(Actor, PerceptionInfo))
		{
			return false;
		}

		const FAISenseID DamageSenseID = UAISense::GetSenseID<UAISense_Damage>();
		for (const FAIStimulus& Stimulus : PerceptionInfo.LastSensedStimuli)
		{
			if (Stimulus.Type == DamageSenseID && Stimulus.WasSuccessfullySensed())
			{
				return true;
			}
		}

		return false;
	}

	bool ContainsActor(const TArray<AActor*>& Actors, const AActor* Actor)
	{
		return Actor && Actors.ContainsByPredicate([Actor](const AActor* Candidate)
		{
			return Candidate == Actor;
		});
	}

	bool HasDirectVisibilityIgnoringOwner(const APawn* Pawn, const APawn* Target)
	{
		if (!Pawn || !Target)
		{
			return false;
		}

		UWorld* World = Pawn->GetWorld();
		if (!World)
		{
			return false;
		}

		const float PawnEyeHeight = Pawn->BaseEyeHeight > 0.f
			? Pawn->BaseEyeHeight
			: Pawn->GetSimpleCollisionHalfHeight() * 0.6f;
		const float TargetEyeHeight = Target->BaseEyeHeight > 0.f
			? Target->BaseEyeHeight
			: Target->GetSimpleCollisionHalfHeight() * 0.6f;
		const FVector Start = Pawn->GetActorLocation() + FVector(0.f, 0.f, PawnEyeHeight);
		const FVector End = Target->GetActorLocation() + FVector(0.f, 0.f, TargetEyeHeight);

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EpicEnemyTargetVisibility), false);
		QueryParams.AddIgnoredActor(Pawn);
		QueryParams.AddIgnoredActor(Target);

		FHitResult Hit;
		return !World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QueryParams);
	}
}

bool FRetrieveEnemyTargetEvaluator::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(AIControllerHandle);
	Linker.LinkExternalData(PawnHandle);
	return true;
}

void FRetrieveEnemyTargetEvaluator::TreeStart(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	
	InstanceData.TargetPlayer = nullptr;
	InstanceData.DistanceToTarget = 0.f;
	InstanceData.bTargetLost = true;
	InstanceData.TimeSinceLastSeen = 0.f;
	// 첫 Tick에서 즉시 쿼리하도록 AccumulatedTime을 임계값으로 초기화
	
	InstanceData.AccumulatedTime = TickInterval;
	
	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);
	if (Pawn)
	{
		InstanceData.SpawnedLocation = Pawn->GetActorLocation();
		
		if (UEnemyCombatComponent* CombatComp = Pawn->GetComponentByClass<UEnemyCombatComponent>())
		{
			InstanceData.CachedCombatComponent = CombatComp;
		}
	}
	
	InstanceData.bOutOfChaseRange = false;
	
	if (ARetrieveEnemyCharacter* Enemy = Cast<ARetrieveEnemyCharacter>(Pawn))
	{
		if (const FMonsterDataRow* Row = Enemy->GetMonsterDataRow())
		{
			InstanceData.AttackableRange = Row->AttackableRange;
			InstanceData.StrafeOffRange = Row->StrafeOffRange;
			InstanceData.StrafeMinNoise = Row->StrafeMinNoise;
			InstanceData.StrafeMaxNoise = Row->StrafeMaxNoise;
			InstanceData.OrbitInnerRadius = Row->OrbitInnerRadius;
			InstanceData.OrbitOuterRadius = Row->OrbitOuterRadius;
			InstanceData.ChaseRange = Row->ChaseRange;
			InstanceData.RechasableRange = Row->RechasableRange;
			InstanceData.bPatrolable = Row->bPatrolable;
			InstanceData.PatrolRange = Row->PatrolRange;
			InstanceData.MoveAcceptableRadius = Row->MoveAcceptableRadius;
			InstanceData.bHasAerialPhase = Enemy->ShouldUseStateTreeAerialPhase()
				&& !Enemy->IsAerialSpecialAttackReady();
		}
	}
}

void FRetrieveEnemyTargetEvaluator::TreeStop(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);
	
	if (Pawn && InstanceData.TargetPlayer)
	{
		if (UEncirclementSubsystem* EncirclementSubsystem = Pawn->GetWorld()->GetSubsystem<UEncirclementSubsystem>())
		{
			EncirclementSubsystem->ReleaseSlot(InstanceData.TargetPlayer, Pawn);
			EncirclementSubsystem->ReleaseAttackToken(InstanceData.TargetPlayer, Pawn);
		}
	}
}

void FRetrieveEnemyTargetEvaluator::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// 위치 갱신은 매 틱 진행하도록 변경
	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);
	if (Pawn && IsValid(InstanceData.TargetPlayer))
	{
		if (IsDeadOrDyingActor(InstanceData.TargetPlayer))
		{
			if (UEncirclementSubsystem* EncirclementSubsystem = Pawn->GetWorld()->GetSubsystem<UEncirclementSubsystem>())
			{
				EncirclementSubsystem->ReleaseSlot(InstanceData.TargetPlayer, Pawn);
				EncirclementSubsystem->ReleaseAttackToken(InstanceData.TargetPlayer, Pawn);
			}

			InstanceData.TargetPlayer = nullptr;
			InstanceData.TargetLocation = FVector::ZeroVector;
			InstanceData.ChaseLocation = FVector::ZeroVector;
			InstanceData.DistanceToTarget = 0.f;
			InstanceData.TimeSinceLastSeen = TargetLostDelay;
			InstanceData.bTargetLost = true;
		}
		else
		{
			const FVector PawnLoc = Pawn->GetActorLocation();
			InstanceData.TargetLocation = InstanceData.TargetPlayer->GetActorLocation();
			InstanceData.DistanceToTarget = FVector2D::Distance(
				FVector2D(PawnLoc.X, PawnLoc.Y),
				FVector2D(InstanceData.TargetLocation.X, InstanceData.TargetLocation.Y)
				);
			const bool bFreezeChaseLocation = InstanceData.CachedCombatComponent.IsValid()
				&& InstanceData.CachedCombatComponent->IsMovementLockedByAttack();
			
			if (!bFreezeChaseLocation)
			{
				const ARetrieveEnemyCharacter* EnemyForMovement = Cast<ARetrieveEnemyCharacter>(Pawn);
				const float DirectChaseRange = FMath::Max(InstanceData.AttackableRange + 35.f, 0.f);
				if (EnemyForMovement && EnemyForMovement->ShouldUseDirectChaseToTarget())
				{
					// 에픽은 단독 교전 비중이 높고 몸집이 커서 포위 슬롯을 찍으면 배회가 과하게 보인다.
					// 타겟을 잡은 동안에는 링 위치보다 플레이어를 직접 추적해 Chase -> Attack 흐름을 우선한다.
					InstanceData.ChaseLocation = InstanceData.TargetLocation;
				}
				else if (InstanceData.DistanceToTarget > DirectChaseRange)
				{
					InstanceData.ChaseLocation = InstanceData.TargetLocation;
				}
				else if (UEncirclementSubsystem* EncSubsystem = Pawn->GetWorld()->GetSubsystem<UEncirclementSubsystem>())
				{
					EncSubsystem->GetOrUpdateRingAnchor(InstanceData.TargetPlayer);
					int32 SlotIndex = EncSubsystem->GetCurrentSlot(InstanceData.TargetPlayer, Pawn);
					if (SlotIndex == INDEX_NONE)
					{
						SlotIndex = EncSubsystem->RequestSlot(InstanceData.TargetPlayer, Pawn);
					}

					if (SlotIndex != INDEX_NONE)
					{
						const bool bHadToken = InstanceData.bHasToken;
						const bool bHasTokenForLocation = EncSubsystem->HasAttackToken(InstanceData.TargetPlayer, Pawn);
						// 토큰을 받을 수 있는(곧 공격할) 적도 안쪽으로 접근해야 사거리에 들어가 공격을 시작할 수 있음
						const bool bCanRequest = EncSubsystem->CanRequestAttackToken(InstanceData.TargetPlayer, Pawn);
						InstanceData.bHasToken = bHasTokenForLocation;
						const bool bUseOuterRadius = !(bHasTokenForLocation || bCanRequest);

						FVector RawTargetLocation = EncSubsystem->GetSlotLocation(InstanceData.TargetPlayer, SlotIndex,
							bUseOuterRadius, InstanceData.StrafeMinNoise, InstanceData.StrafeMaxNoise,
							InstanceData.OrbitInnerRadius, InstanceData.OrbitOuterRadius,
							InstanceData.StrafeOffRange * 0.9f); // 대기자는 Strafe 범위 내에 머무름

						const float JumpSq = FVector::DistSquared(InstanceData.ChaseLocation, RawTargetLocation);
						if (InstanceData.ChaseLocation.IsNearlyZero() || bHadToken != bHasTokenForLocation || JumpSq > FMath::Square(120.f))
						{
							InstanceData.ChaseLocation = RawTargetLocation;
						}
						else
						{
							InstanceData.ChaseLocation = FMath::VInterpTo(
								InstanceData.ChaseLocation, RawTargetLocation, DeltaTime, 7.f);
						}
					}
					else
					{
						InstanceData.ChaseLocation = EncSubsystem->GetOverflowStandoffLocation(
							InstanceData.TargetPlayer, Pawn, InstanceData.StrafeOffRange);
					}
				}
				else
				{
					InstanceData.ChaseLocation = InstanceData.TargetLocation;
				}
			}
		}
	}
	
	// 태그 갱신도 매 틱 진행
	if (const IAbilitySystemInterface* ASCIf = Cast<IAbilitySystemInterface>(Pawn))
	{
		if (UAbilitySystemComponent* ASC = ASCIf->GetAbilitySystemComponent())
		{
			InstanceData.OwnedTags.Reset();
			ASC->GetOwnedGameplayTags(InstanceData.OwnedTags);
		}
	}

	if (InstanceData.CachedCombatComponent.IsValid())
	{
		const bool bPatternActive = InstanceData.CachedCombatComponent->IsPatternActive();
		if (bPatternActive)
		{
			InstanceData.bSpecialAttackable = false;
			InstanceData.bAttackable = false;
		}
		else if (InstanceData.CachedCombatComponent->IsSpecialAttackEvaluationLocked())
		{
			InstanceData.bSpecialAttackable = false;
		}
	}
	
	InstanceData.AccumulatedTime += DeltaTime;
	if (InstanceData.AccumulatedTime < TickInterval)
	{
		return;
	}
	InstanceData.AccumulatedTime = 0.f;

	AAIController* AIController = Context.GetExternalDataPtr(AIControllerHandle);
	if (!AIController || !Pawn)
	{
		return;
	}

	UAIPerceptionComponent* PerceptionComp = AIController->GetAIPerceptionComponent();
	if (!PerceptionComp)
	{
		return;
	}
	
	const float DistanceFromSpawn =
	FVector::Dist(InstanceData.SpawnedLocation, Pawn->GetActorLocation());
	
	const bool bNewOutOfChaseRange = DistanceFromSpawn >= InstanceData.ChaseRange;

	if (InstanceData.bWasOutOfChaseRange && !bNewOutOfChaseRange)
	{
		// 원점 복귀 완료 → 타깃 즉시 초기화
		if (UEncirclementSubsystem* EncirclementSubsystem = Pawn->GetWorld()->GetSubsystem<UEncirclementSubsystem>())
		{
			EncirclementSubsystem->ReleaseSlot(InstanceData.TargetPlayer, Pawn);
			EncirclementSubsystem->ReleaseAttackToken(InstanceData.TargetPlayer, Pawn);
		}
		InstanceData.TargetPlayer   = nullptr;
		InstanceData.TimeSinceLastSeen = TargetLostDelay;
		InstanceData.bTargetLost    = true;
	}

	InstanceData.bWasOutOfChaseRange = bNewOutOfChaseRange;
	InstanceData.bOutOfChaseRange = bNewOutOfChaseRange;

	TArray<AActor*> PerceivedActors;
	PerceptionComp->GetKnownPerceivedActors(nullptr, PerceivedActors);
	
	const FVector PawnLocation = Pawn->GetActorLocation();

	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(Pawn, 0))
	{
		const ARetrieveEnemyCharacter* EnemyForAcquisition = Cast<ARetrieveEnemyCharacter>(Pawn);
		const float AcquireRangeMultiplier = EnemyForAcquisition
			? EnemyForAcquisition->GetInitialAcquireRangeMultiplierForAI()
			: 1.f;
		const float PlayerDistSq = FVector::DistSquared(PawnLocation, PlayerPawn->GetActorLocation());
		const float BaseInitialAcquireRange = InstanceData.ChaseRange > 0.f ? InstanceData.ChaseRange : 1500.f;
		const float InitialAcquireRange = BaseInitialAcquireRange * FMath::Max(1.f, AcquireRangeMultiplier);
		const bool bHasLineOfSight = AIController->LineOfSightTo(PlayerPawn)
			|| (EnemyForAcquisition
				&& EnemyForAcquisition->ShouldUseDirectVisibilityTargetAcquisition()
				&& HasDirectVisibilityIgnoringOwner(Pawn, PlayerPawn));
		if (PlayerDistSq <= FMath::Square(InitialAcquireRange)
			&& bHasLineOfSight
			&& !ContainsActor(PerceivedActors, PlayerPawn))
		{
			PerceivedActors.Add(PlayerPawn);
		}
	}

	AActor* BestTarget = nullptr;
	float BestScore = MAX_FLT;
	float CurrentScore = MAX_FLT;
	const bool bApplyFov = (InstanceData.TargetPlayer == nullptr);

	UEncirclementSubsystem* EncirclementSubsystem = Pawn->GetWorld()->GetSubsystem<UEncirclementSubsystem>();

	for (AActor* Actor : PerceivedActors)
	{
		if (!Actor || IsDeadOrDyingActor(Actor))
		{
			continue;
		}
		const AAIController* OwnerController = Cast<AAIController>(Pawn->GetController());
		if (!OwnerController ||
			OwnerController->GetTeamAttitudeTowards(*Actor) != ETeamAttitude::Hostile)
		{
			continue;
		}

		const bool bDamageSensed = WasDamageSensed(PerceptionComp, Actor);
		if (bApplyFov && !bDamageSensed)
		{
			FVector ToTarget = Actor->GetActorLocation() - PawnLocation;
			FVector Forward = Pawn->GetActorForwardVector();
			ToTarget.Z = 0.f;
			Forward.Z = 0.f;
			if (!ToTarget.IsNearlyZero() && !Forward.IsNearlyZero())
			{
				const ARetrieveEnemyCharacter* EnemyForFOV = Cast<ARetrieveEnemyCharacter>(Pawn);
				const float HalfFOVOverride = EnemyForFOV
					? EnemyForFOV->GetHorizontalHalfFOVOverrideForAI()
					: -1.f;
				const float EffectiveHalfFOV = HalfFOVOverride > 0.f
					? FMath::Max(HorizontalHalfFOV, HalfFOVOverride)
					: HorizontalHalfFOV;
				const float CosHalfFov = FMath::Cos(FMath::DegreesToRadians(EffectiveHalfFOV));
				ToTarget.Normalize();
				if (FVector::DotProduct(Forward, ToTarget) < CosHalfFov)
				{
					continue;
				}
			}
		}

		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Pawn->GetWorld());
		if (NavSys)
		{
			FNavLocation ProjectedLoc;
			if (!NavSys->ProjectPointToNavigation(Actor->GetActorLocation(), ProjectedLoc,
			                                      FVector(100.f, 100.f, 250.f)))
			{
				continue;
			}
		}

		const float DistSq = FVector::DistSquared(PawnLocation, Actor->GetActorLocation());
		const int32 Committed = EncirclementSubsystem ? EncirclementSubsystem->GetCommittedCount(Actor) : 0;
		const float Score = DistSq * (1.f + AggroCrowdWeight * Committed);

		if (Actor == InstanceData.TargetPlayer)
		{
			CurrentScore = Score;
		}
		if (Score < BestScore)
		{
			BestScore = Score;
			BestTarget = Actor;
		}
	}

	AActor* ChosenTarget = BestTarget;
	if (IsValid(InstanceData.TargetPlayer) && CurrentScore < MAX_FLT)
	{
		if (BestTarget != InstanceData.TargetPlayer && BestScore > CurrentScore * TargetSwitchHysteresis)
		{
			ChosenTarget = InstanceData.TargetPlayer; // 충분히 개선되지 않음 → 유지
		}
	}

	if (ChosenTarget)
	{
		if (InstanceData.TargetPlayer != ChosenTarget)
		{
			const bool bHadTarget = InstanceData.TargetPlayer != nullptr;
			if (EncirclementSubsystem && InstanceData.TargetPlayer)
			{
				EncirclementSubsystem->ReleaseSlot(InstanceData.TargetPlayer, Pawn);
				EncirclementSubsystem->ReleaseAttackToken(InstanceData.TargetPlayer, Pawn);
			}
			InstanceData.TargetPlayer = ChosenTarget;

			if (!bHadTarget)
			{
				FEnemyPlayerSpottedPayload Payload;
				Payload.SpottedActor = ChosenTarget;
				Payload.SpottedLocation = ChosenTarget->GetActorLocation();
				Payload.InstigatorLocation = Pawn->GetActorLocation();
				Payload.InstigatorEnemy = Pawn;
				UGameplayMessageSubsystem::Get(Pawn->GetWorld()).BroadcastMessage(
					RetrieveGameplayTags::Channel_Enemy_PlayerSpotted, Payload);
			}
		}
		InstanceData.TimeSinceLastSeen = 0.f;
		InstanceData.bTargetLost = false;
	}
	else if (InstanceData.TargetPlayer)
	{
		InstanceData.TimeSinceLastSeen += TickInterval;
		if (InstanceData.TimeSinceLastSeen >= TargetLostDelay)
		{
			if (EncirclementSubsystem)
			{
				EncirclementSubsystem->ReleaseSlot(InstanceData.TargetPlayer, Pawn);
				EncirclementSubsystem->ReleaseAttackToken(InstanceData.TargetPlayer, Pawn);
			}
			InstanceData.TargetPlayer = nullptr;
			InstanceData.DistanceToTarget = 0.f;
			InstanceData.bTargetLost = true;
		}
	}
	else
	{
		if (ARetrieveEnemyCharacter* EnemyChar = Cast<ARetrieveEnemyCharacter>(Pawn))
		{
			if (AActor* Alerted = EnemyChar->AlertedTarget)
			{
				if (!IsDeadOrDyingActor(Alerted))
				{
					InstanceData.TargetPlayer = Alerted;
					InstanceData.bTargetLost = false;
				}
				EnemyChar->AlertedTarget = nullptr;
			}
		}
	}
	
	if (InstanceData.CachedCombatComponent.IsValid())
	{
		if (UEncirclementSubsystem* EncircleSubsystem = Pawn->GetWorld()->GetSubsystem<UEncirclementSubsystem>())
		{
			const bool bHasValidTarget = IsValid(InstanceData.TargetPlayer);
			
			InstanceData.bHasToken = bHasValidTarget
				&& EncircleSubsystem->HasAttackToken(InstanceData.TargetPlayer, Pawn);

			const bool bCanRequestToken = bHasValidTarget
				&& EncircleSubsystem->CanRequestAttackToken(InstanceData.TargetPlayer, Pawn);

			const bool bPatternActive = InstanceData.CachedCombatComponent->IsPatternActive();
			const bool bSpecialAttackEvaluationLocked =
				InstanceData.CachedCombatComponent->IsSpecialAttackEvaluationLocked();
			const bool bSpecialAttackRetryCooldownReady =
				InstanceData.CachedCombatComponent->IsSpecialAttackRetryCooldownReady();

			const ARetrieveEnemyCharacter* EnemyCharacter = Cast<ARetrieveEnemyCharacter>(Pawn);
			const UCharacterMovementComponent* CharacterMovement = Pawn->FindComponentByClass<UCharacterMovementComponent>();
			const bool bShouldSuppressNormalAttackWhileFlying = EnemyCharacter
				&& EnemyCharacter->ShouldSuppressNormalAttackWhileFlying()
				&& CharacterMovement
				&& CharacterMovement->MovementMode == MOVE_Flying;
			const bool bUsePatternRangeForNormalAttack = EnemyCharacter
				&& EnemyCharacter->ShouldUsePatternRangeForNormalAttack();
			const bool bAttackPatternAvailable =
				InstanceData.CachedCombatComponent->IsAttackable(InstanceData.TargetPlayer);
			const bool bNormalAttackable = bCanRequestToken
				&& !bPatternActive
				&& !bShouldSuppressNormalAttackWhileFlying
				&& bAttackPatternAvailable
				&& (bUsePatternRangeForNormalAttack || InstanceData.DistanceToTarget <= InstanceData.AttackableRange);

			// 특수 공격 가능 여부 — 일반/보스는 원본 동작 그대로(일반 공격 가능 여부와 독립적으로
			// 쿨다운/락만으로 판정). 에픽도 동일하게 쿨다운 완료 시 발동 가능.
			const bool bRawSpecialAttackable = bHasValidTarget
				&& !bPatternActive
				&& !bSpecialAttackEvaluationLocked
				&& bSpecialAttackRetryCooldownReady
				&& InstanceData.CachedCombatComponent->HasAvailablePatternByType(
					InstanceData.TargetPlayer, RetrieveGameplayTags::Ability_Enemy_SpecialAttack);

			InstanceData.bSpecialAttackable = bRawSpecialAttackable;

			if (EnemyCharacter && EnemyCharacter->ShouldUsePatternRangeForNormalAttack())
			{
				InstanceData.bHasAerialPhase = EnemyCharacter
					&& EnemyCharacter->ShouldUseStateTreeAerialPhase()
					&& !EnemyCharacter->IsAerialSpecialAttackReady()
					&& bRawSpecialAttackable;
				const bool bEpicHasToken = bHasValidTarget
					&& EncircleSubsystem->HasAttackToken(InstanceData.TargetPlayer, Pawn);
				InstanceData.bHasToken = bEpicHasToken;
				InstanceData.bAttackable = bNormalAttackable;
			}
			else
			{
				// 일반/보스: 원본 동작 그대로. 일반 공격과 특수 공격 판정은 서로 독립적이며
				// StateTree 전이 우선순위가 선택을 담당한다.
				InstanceData.bHasAerialPhase = false;
				InstanceData.bAttackable = bNormalAttackable;
			}
		}
		else
		{
			InstanceData.bHasToken   = false;
			InstanceData.bAttackable = false;
			InstanceData.bSpecialAttackable = false;
			InstanceData.bHasAerialPhase = false;
		}
	}
	else
	{
		InstanceData.bHasToken   = false;
		InstanceData.bAttackable = false;
		InstanceData.bSpecialAttackable = false;
		InstanceData.bHasAerialPhase = false;
	}
}
