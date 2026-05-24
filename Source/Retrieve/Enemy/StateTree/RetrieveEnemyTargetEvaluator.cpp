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
	InstanceData.bTargetLost = false;
	InstanceData.TimeSinceLastSeen = 0.f;
	// 첫 Tick에서 즉시 쿼리하도록 AccumulatedTime을 임계값으로 초기화
	InstanceData.AccumulatedTime = TickInterval;
	
	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);
	InstanceData.SpawnedLocation = Pawn->GetActorLocation();
	
	InstanceData.bOutOfChaseRange = false;
}

void FRetrieveEnemyTargetEvaluator::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	InstanceData.AccumulatedTime += DeltaTime;
	if (InstanceData.AccumulatedTime < TickInterval)
	{
		return;
	}
	InstanceData.AccumulatedTime = 0.f;

	AAIController* AIController = Context.GetExternalDataPtr(AIControllerHandle);
	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);
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

	InstanceData.bOutOfChaseRange = DistanceFromSpawn >= InstanceData.ChaseRange;
	
	TArray<AActor*> PerceivedActors;
	PerceptionComp->GetCurrentlyPerceivedActors(nullptr, PerceivedActors);

	AActor* NearestTarget  = nullptr;
	float NearestDistSq  = MAX_FLT;
	const FVector PawnLocation = Pawn->GetActorLocation();

	for (AActor* Actor : PerceivedActors)
	{
		if (!Actor)
		{
			continue;
		}
		const AAIController* OwnerController =
			Cast<AAIController>(Pawn->GetController());

		if (!OwnerController ||
			OwnerController->GetTeamAttitudeTowards(*Actor) != ETeamAttitude::Hostile)
		{
			return;
		}
		
		const float DistSq = FVector::DistSquared(PawnLocation, Actor->GetActorLocation());
		if (DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			NearestTarget = Actor;
		}
	}

	if (NearestTarget)
	{
		if (InstanceData.TargetPlayer != NearestTarget)
		{
			const bool bHadTarget = InstanceData.TargetPlayer != nullptr;
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
				InstanceData.TargetPlayer = Alerted;
				InstanceData.bTargetLost  = false;
				EnemyChar->AlertedTarget  = nullptr;
			}
		}
	}
	
	if (IsValid(InstanceData.TargetPlayer))
	{
		InstanceData.TargetLocation = InstanceData.TargetPlayer->GetActorLocation();
		InstanceData.DistanceToTarget = FVector::Dist(Pawn->GetActorLocation(), InstanceData.TargetLocation);
	}
	
	if (const IAbilitySystemInterface* ASCIf = Cast<IAbilitySystemInterface>(Pawn))
	{
		if (UAbilitySystemComponent* ASC = ASCIf->GetAbilitySystemComponent())
		{
			InstanceData.OwnedTags.Reset();
			ASC->GetOwnedGameplayTags(InstanceData.OwnedTags);
		}
	}
}
