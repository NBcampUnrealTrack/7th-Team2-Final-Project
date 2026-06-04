#include "Enemy/StateTree/RetrieveEnemyTargetEvaluator.h"

#include "StateTreeLinker.h"
#include "StateTreeExecutionContext.h"
#include "AIController.h"
#include "Perception/AIPerceptionComponent.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayMessages/RetrieveGameplayMessageTypes.h"
#include "Character/RetrieveEnemyCharacter.h"
#include "NavigationSystem.h"
#include "Components/RetrieveHealthComponent.h"
#include "Enemy/EncirclementSubsystem.h"

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
	InstanceData.SpawnedLocation = Pawn->GetActorLocation();
	
	InstanceData.bOutOfChaseRange = false;
	InstanceData.SlotIndex = INDEX_NONE;
}

void FRetrieveEnemyTargetEvaluator::TreeStop(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);
	
	if (Pawn && InstanceData.TargetPlayer)
	{
		if (UEncirclementSubsystem* EncSubsystem = Pawn->GetWorld()->GetSubsystem<UEncirclementSubsystem>())
		{
			EncSubsystem->ReleaseSlot(InstanceData.TargetPlayer, Pawn);
		}
	}
	
	InstanceData.SlotIndex = INDEX_NONE;
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
			if (UEncirclementSubsystem* EncirComp = Pawn->GetWorld()->GetSubsystem<UEncirclementSubsystem>())
			{
				EncirComp->ReleaseSlot(InstanceData.TargetPlayer, Pawn);
			}

			InstanceData.TargetPlayer = nullptr;
			InstanceData.TargetLocation = FVector::ZeroVector;
			InstanceData.ChaseLocation = FVector::ZeroVector;
			InstanceData.DistanceToTarget = 0.f;
			InstanceData.TimeSinceLastSeen = TargetLostDelay;
			InstanceData.bTargetLost = true;
			InstanceData.SlotIndex = INDEX_NONE;
		}
		else
		{
			const FVector PawnLoc   = Pawn->GetActorLocation();
			InstanceData.TargetLocation   = InstanceData.TargetPlayer->GetActorLocation();
			InstanceData.DistanceToTarget = InstanceData.DistanceToTarget = FVector2D::Distance(
				FVector2D(PawnLoc.X, PawnLoc.Y),
				FVector2D(InstanceData.TargetLocation.X, InstanceData.TargetLocation.Y)
)			;
		
			if (UEncirclementSubsystem* EncSubsystem = Pawn->GetWorld()->GetSubsystem<UEncirclementSubsystem>())
			{
				if (InstanceData.SlotIndex == INDEX_NONE)   // ★ 슬롯 없으면 즉시 요청 (알림 등 모든 경로 커버)
				{
					InstanceData.SlotIndex = EncSubsystem->RequestSlot(InstanceData.TargetPlayer, Pawn);
				}
			
				InstanceData.ChaseLocation = (InstanceData.SlotIndex != INDEX_NONE)
					? EncSubsystem->GetSlotLocation(InstanceData.TargetPlayer, InstanceData.SlotIndex)
					: InstanceData.TargetLocation;
			}
			else
			{
				InstanceData.ChaseLocation = InstanceData.TargetLocation;
			}
		}
	}
	
	if (const IAbilitySystemInterface* ASCIf = Cast<IAbilitySystemInterface>(Pawn))
	{
		if (UAbilitySystemComponent* ASC = ASCIf->GetAbilitySystemComponent())
		{
			InstanceData.OwnedTags.Reset();
			ASC->GetOwnedGameplayTags(InstanceData.OwnedTags);
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

	// InstanceData.bOutOfChaseRange = DistanceFromSpawn >= InstanceData.ChaseRange;
	/*UE_LOG(LogTemp, Warning, TEXT("[%s] DistFromSpawn=%.0f ChaseRange=%.0f bOutOfChaseRange=%d"),
	*Pawn->GetName(), DistanceFromSpawn, InstanceData.ChaseRange, InstanceData.bOutOfChaseRange ? 1 : 0);
	*/
	const bool bNewOutOfChaseRange = DistanceFromSpawn >= InstanceData.ChaseRange;

	if (InstanceData.bWasOutOfChaseRange && !bNewOutOfChaseRange)
	{
		// 원점 복귀 완료 → 타깃 즉시 초기화
		if (UEncirclementSubsystem* Enc = Pawn->GetWorld()->GetSubsystem<UEncirclementSubsystem>())
		{
			Enc->ReleaseSlot(InstanceData.TargetPlayer, Pawn);
		}
		InstanceData.TargetPlayer   = nullptr;
		InstanceData.TimeSinceLastSeen = TargetLostDelay;
		InstanceData.bTargetLost    = true;
		InstanceData.SlotIndex      = INDEX_NONE;
	}

	InstanceData.bWasOutOfChaseRange = bNewOutOfChaseRange;
	InstanceData.bOutOfChaseRange    = bNewOutOfChaseRange;
	
	TArray<AActor*> PerceivedActors;
	PerceptionComp->GetKnownPerceivedActors(nullptr, PerceivedActors);
	
	AActor* NearestTarget  = nullptr;
	float NearestDistSq  = MAX_FLT;
	const FVector PawnLocation = Pawn->GetActorLocation();
	const bool bApplyFov = (InstanceData.TargetPlayer == nullptr);
	
	for (AActor* Actor : PerceivedActors)
	{
		if (!Actor || IsDeadOrDyingActor(Actor))
		{
			continue;
		}
		const AAIController* OwnerController =
			Cast<AAIController>(Pawn->GetController());
		
		if (!OwnerController ||
			OwnerController->GetTeamAttitudeTowards(*Actor) != ETeamAttitude::Hostile)
		{
			continue;
		}

		if (bApplyFov)   // 이미 추적 중이면 시야 무시하고 계속 추적
		{
			FVector ToTarget = Actor->GetActorLocation() - PawnLocation;
			FVector Forward  = Pawn->GetActorForwardVector();
			ToTarget.Z = 0.f;
			Forward.Z = 0;
			
			if (!ToTarget.IsNearlyZero() && !Forward.IsNearlyZero())
			{
				const float CosHalfFov = FMath::Cos(FMath::DegreesToRadians(HorizontalHalfFOV));
				ToTarget.Normalize();
				
				if (FVector::DotProduct(Forward, ToTarget) < CosHalfFov)
				{
					continue;   // 수평 시야 밖 → 무시 (획득 시에만)
				}
			}
		}
		
		const float DistSq = FVector::DistSquared(PawnLocation, Actor->GetActorLocation());
		if (DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			NearestTarget = Actor;
		}
	}
	
	/*if (NearestTarget)
	{
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Pawn->GetWorld());
		FNavLocation ProjectedLoc;
		const bool bOnNavMesh = NavSys && NavSys->ProjectPointToNavigation(
			NearestTarget->GetActorLocation(), ProjectedLoc, FVector(100.f, 100.f, 250.f));
		if (!bOnNavMesh)
		{
			NearestTarget = nullptr;   // 보여도 NavMesh 밖이면 "안 보이는 것"으로 처리
		}
	}	*/
		
	if (NearestTarget)
	{
		if (InstanceData.TargetPlayer != NearestTarget)
		{
			const bool bHadTarget = InstanceData.TargetPlayer != nullptr;
			
			if (UEncirclementSubsystem* Enc = Pawn->GetWorld()->GetSubsystem<UEncirclementSubsystem>())
			{
				if (InstanceData.TargetPlayer)
				{
					Enc->ReleaseSlot(InstanceData.TargetPlayer, Pawn);
				}
				InstanceData.SlotIndex = NearestTarget ? Enc->RequestSlot(NearestTarget, Pawn) : INDEX_NONE;
			}
			
			InstanceData.TargetPlayer = NearestTarget;
			
			if (!bHadTarget)
			{
				FEnemyPlayerSpottedPayload Payload;
				Payload.SpottedActor = NearestTarget;
				Payload.SpottedLocation = NearestTarget->GetActorLocation();
				Payload.InstigatorLocation = Pawn->GetActorLocation();
				Payload.InstigatorEnemy = Pawn;

				UWorld* World = Pawn->GetWorld();
				UGameplayMessageSubsystem& MsgSubsys = UGameplayMessageSubsystem::Get(World);
				MsgSubsys.BroadcastMessage(
					RetrieveGameplayTags::Channel_Enemy_PlayerSpotted,
					Payload);
			}
		}
		InstanceData.TimeSinceLastSeen = 0.f;
		InstanceData.bTargetLost = false;
	}
	else if (InstanceData.TargetPlayer)
	{
		// 타깃이 인지 범위를 벗어난 상태 — 유예 시간 누적
		InstanceData.TimeSinceLastSeen += TickInterval;
		if (InstanceData.TimeSinceLastSeen >= TargetLostDelay)
		{
			if (UEncirclementSubsystem* Enc = Pawn->GetWorld()->GetSubsystem<UEncirclementSubsystem>())
			{
				Enc->ReleaseSlot(InstanceData.TargetPlayer, Pawn);
			}
			InstanceData.SlotIndex = INDEX_NONE;
			
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
					InstanceData.bTargetLost  = false;
				}
				
				EnemyChar->AlertedTarget  = nullptr;
			}
		}
	}
}
