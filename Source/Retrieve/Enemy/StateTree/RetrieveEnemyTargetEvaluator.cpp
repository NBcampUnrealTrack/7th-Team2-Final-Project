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
#include "Messaging/GameplayMessages/RetrieveGameplayMessageTypes.h"
#include "Character/RetrieveEnemyCharacter.h"
#include "NavigationSystem.h"
#include "Components/Combat/RetrieveHealthComponent.h"
#include "Enemy/EncirclementSubsystem.h"
#include "Components/Enemy/EnemyCombatComponent.h"
#include "Data/RetrieveDataTableTypes.h"
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
				if (UEncirclementSubsystem* EncSubsystem = Pawn->GetWorld()->GetSubsystem<UEncirclementSubsystem>())
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

	AActor* BestTarget = nullptr;
	float BestScore = MAX_FLT;
	float CurrentScore = MAX_FLT;
	const FVector PawnLocation = Pawn->GetActorLocation();
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
				const float CosHalfFov = FMath::Cos(FMath::DegreesToRadians(HorizontalHalfFOV));
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

			InstanceData.bSpecialAttackable = bCanRequestToken
				&& !bPatternActive
				&& !bSpecialAttackEvaluationLocked
				&& bSpecialAttackRetryCooldownReady
				&& InstanceData.CachedCombatComponent->HasAvailablePatternByType(
					InstanceData.TargetPlayer, RetrieveGameplayTags::Ability_Enemy_SpecialAttack);
			
			InstanceData.bAttackable =
				bCanRequestToken
				&& !bPatternActive
				&& InstanceData.DistanceToTarget <= InstanceData.AttackableRange
				&& InstanceData.CachedCombatComponent->IsAttackable(InstanceData.TargetPlayer);
		}
		else
		{
			InstanceData.bHasToken   = false;
			InstanceData.bAttackable = false;
			InstanceData.bSpecialAttackable = false;
		}
	}
	else
	{
		InstanceData.bHasToken   = false;
		InstanceData.bAttackable = false;
		InstanceData.bSpecialAttackable = false;
	}
}
