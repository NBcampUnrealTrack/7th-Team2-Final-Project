#include "Enemy/StateTree/RetrieveEnemyTargetEvaluator.h"

#include "StateTreeLinker.h"
#include "StateTreeExecutionContext.h"
#include "AIController.h"
#include "Enemy/EnemyAIController.h"
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
#include "Components/Enemy/EnemySuspicionIndicatorComponent.h"
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

		if (UEnemySuspicionIndicatorComponent* SuspicionIndicator = Pawn->GetComponentByClass<UEnemySuspicionIndicatorComponent>())
		{
			InstanceData.CachedSuspicionIndicator = SuspicionIndicator;
		}
	}
	
	InstanceData.bOutOfChaseRange = false;
	
	if (ARetrieveEnemyCharacter* Enemy = Cast<ARetrieveEnemyCharacter>(Pawn))
	{
		InstanceData.bUseDirectChaseToTarget = Enemy->ShouldUseDirectChaseToTarget();
	
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
			InstanceData.SuspicionIncreaseRate = Row->SuspicionIncreaseRate;
			InstanceData.SuspicionDecreaseRate = Row->SuspicionDecreaseRate;
			InstanceData.ForceCombatRange = Row->ForceCombatRange;
			InstanceData.bPatrolable = Row->bPatrolable;
			InstanceData.PatrolRange = Row->PatrolRange;
			InstanceData.MoveAcceptableRadius = Row->MoveAcceptableRadius;
			if (Row->bOverrideDirectChaseToTarget)
			{
				InstanceData.bUseDirectChaseToTarget = Row->bUseDirectChaseToTarget;
			}
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
				// 플레이어에게 무조건 붙는 이동을 피하게 수정
				const float DirectChaseRange = FMath::Max(InstanceData.AttackableRange + 35.f, 0.f);
				if (InstanceData.bUseDirectChaseToTarget)
				{
					// DataTable/캐릭터 기본 설정에서 직접 추적을 허용한 몬스터는 플레이어 위치를 ChaseLocation으로 사용.
					// 직접 추적을 끈 몬스터는 전투 거리 안에서 오빗/스트레이프 위치를 사용
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
		const float PlayerDistSq = FVector::DistSquared(PawnLocation, PlayerPawn->GetActorLocation());
		float InitialAcquireRange;
		if (InstanceData.TargetPlayer == nullptr)
		{
			// 최초 발견은 AIPerception Sight 설정(SightRadius)을 그대로 따른다.
			const AEnemyAIController* EnemyAIController = Cast<AEnemyAIController>(AIController);
			InitialAcquireRange = EnemyAIController
				? EnemyAIController->GetEffectiveSightRadius()
				: InstanceData.ChaseRange;
		}
		else
		{
			// 이미 추적 중인 타겟은 ChaseRange까지 재획득을 유지한다(추격 중 시야 이탈 방지).
			InitialAcquireRange = InstanceData.ChaseRange > 0.f ? InstanceData.ChaseRange : 1500.f;
		}
		if (PlayerDistSq <= FMath::Square(InitialAcquireRange)
			&& AIController->LineOfSightTo(PlayerPawn)
			&& !ContainsActor(PerceivedActors, PlayerPawn))
		{
			PerceivedActors.Add(PlayerPawn);
		}
	}

	AActor* BestTarget = nullptr;
	float BestScore = MAX_FLT;
	float CurrentScore = MAX_FLT;
	// 경계(Suspicious) 단계를 쓰는 몬스터는 최초 발견에 정면 시야각을 요구하지 않는다 —
	// 거리 내에 들어오면 우선 눈치채고, Suspicious 상태에서 서서히 돌아보게 한다.
	const bool bUsesSuspiciousFlow = InstanceData.SuspicionIncreaseRate > 0.f;
	const bool bApplyFov = (InstanceData.TargetPlayer == nullptr) && !bUsesSuspiciousFlow;

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

			// Suspicious 플로우를 쓰는 몬스터는 여기서 바로 전파하지 않는다 —
			// 경계 게이지가 다 찼을 때(진짜 Combat 진입 시점)로 미룬다 (아래 참고).
			if (!bHadTarget && !bUsesSuspiciousFlow)
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
		if (bUsesSuspiciousFlow && !InstanceData.bSuspicionGaugeFull)
		{
			// 아직 Combat까지 확신하지 못한 Suspicious 단계에서는 유예 없이 즉시 놓친다.
			if (EncirclementSubsystem)
			{
				EncirclementSubsystem->ReleaseSlot(InstanceData.TargetPlayer, Pawn);
				EncirclementSubsystem->ReleaseAttackToken(InstanceData.TargetPlayer, Pawn);
			}
			InstanceData.TargetPlayer = nullptr;
			InstanceData.DistanceToTarget = 0.f;
			InstanceData.bTargetLost = true;
		}
		else
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
					// 동료가 이미 확신하고 전파한 대상이므로 Suspicious를 건너뛰고 바로 Combat 자격을 준다.
					InstanceData.SuspicionGauge = 1.f;
				}
				EnemyChar->AlertedTarget = nullptr;
			}
		}
	}

	if (bUsesSuspiciousFlow)
	{
		if (IsValid(InstanceData.TargetPlayer) && !InstanceData.bTargetLost)
		{
			// DistanceToTarget은 타겟을 처음 인식한 이번 틱엔 아직 갱신 전(이전 틱 값=0)이라
			// 여기서 직접 신선한 거리를 계산한다 — 안 그러면 최초 인식 시 항상 0<=ForceCombatRange로 오판된다.
			const float FreshDistanceToTarget = FVector2D::Distance(
				FVector2D(Pawn->GetActorLocation()), FVector2D(InstanceData.TargetPlayer->GetActorLocation()));
			const bool bForceCombat = InstanceData.ForceCombatRange > 0.f
				&& FreshDistanceToTarget <= InstanceData.ForceCombatRange;
			if (WasDamageSensed(PerceptionComp, InstanceData.TargetPlayer) || bForceCombat)
			{
				// 피격 감지 또는 강제 전투 진입 거리 이내 — 이미 확실히 들켰으므로 Suspicious를 건너뛴다.
				InstanceData.SuspicionGauge = 1.f;
			}
			else
			{
				InstanceData.SuspicionGauge = FMath::Clamp(
					InstanceData.SuspicionGauge + InstanceData.SuspicionIncreaseRate * TickInterval, 0.f, 1.f);
			}
		}
		else if (InstanceData.SuspicionGauge > 0.f)
		{
			InstanceData.SuspicionGauge = FMath::Clamp(
				InstanceData.SuspicionGauge - InstanceData.SuspicionDecreaseRate * TickInterval, 0.f, 1.f);
		}

		InstanceData.bSuspicionGaugeFull = InstanceData.SuspicionGauge >= 1.f;

		if (InstanceData.bSuspicionGaugeFull && !InstanceData.bWasSuspicionGaugeFull && IsValid(InstanceData.TargetPlayer))
		{
			FEnemyPlayerSpottedPayload Payload;
			Payload.SpottedActor = InstanceData.TargetPlayer;
			Payload.SpottedLocation = InstanceData.TargetPlayer->GetActorLocation();
			Payload.InstigatorLocation = Pawn->GetActorLocation();
			Payload.InstigatorEnemy = Pawn;
			UGameplayMessageSubsystem::Get(Pawn->GetWorld()).BroadcastMessage(
				RetrieveGameplayTags::Channel_Enemy_PlayerSpotted, Payload);
		}
		InstanceData.bWasSuspicionGaugeFull = InstanceData.bSuspicionGaugeFull;

		if (InstanceData.CachedSuspicionIndicator.IsValid())
		{
			InstanceData.CachedSuspicionIndicator->SetSuspicionGauge(InstanceData.SuspicionGauge);
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
