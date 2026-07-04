#include "LumenContextEvaluator.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AIController.h"
#include "EngineUtils.h"
#include "GenericTeamAgentInterface.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"
#include "Lumen/LumenFollowComponent.h"
#include "Core/RetrieveGameState.h"
#include "GameplayTags/RetrieveGameplayTags.h"

class ARetrieveGameState;

bool FLumenContextEvaluator::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(AIControllerHandle);
	Linker.LinkExternalData(PawnHandle);
	return true;
}

void FLumenContextEvaluator::TreeStart(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (APawn* Pawn = Context.GetExternalDataPtr(PawnHandle))
	{
		InstanceData.CachedFollowComp = Pawn->FindComponentByClass<ULumenFollowComponent>();
	}
}

void FLumenContextEvaluator::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);
	if (!Pawn)
	{
		return;
	}
	UWorld* World = Pawn->GetWorld();

	// ---- 호스트 폰 및 위치/거리를 수집
	APawn* Host = nullptr;
	if (ARetrieveGameState* GS = World ? World->GetGameState<ARetrieveGameState>() : nullptr)
	{
		Host = GS->GetHostPawn();
	}
	InstanceData.HostActor = Host;

	if (Host)
	{
		InstanceData.HostLocation = Host->GetActorLocation();
		InstanceData.DistanceToHost = FVector::Distance(Host->GetActorLocation(), Pawn->GetActorLocation());

		// 호스트의 전투 여부. 루멘은 ASC가 없으므로 호스트 PlayerState의 Sovereign ASC에서 읽는다.
		bool bStanceCombat = false;
		if (const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Host))
		{
			if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
			{
				bStanceCombat = ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Combat);
			}
		}
		InstanceData.bHostInCombat = bStanceCombat || InstanceData.bHostEngaged;
	}
	else
	{
		InstanceData.DistanceToHost = 0.f;
		InstanceData.bHostInCombat = false;
		InstanceData.bHostEngaged = false;
	}

	// ---- 플레이어가 Wait 토글을 켰는지 수집
	InstanceData.bWaitRequested = InstanceData.CachedFollowComp.IsValid() && InstanceData.CachedFollowComp->IsWaitRequested();

	// ---- 주변 반경 내에 적대 액터가 있는지 수집
	// TODO: 월드 액터 전체 순회라 비용이 큼. 일단 일정 주기로만 스캔하도록 조치했는데 다른 최적화 방안 고려해볼것.
	InstanceData.TimeSinceThreatScan += DeltaTime;
	if (InstanceData.TimeSinceThreatScan >= ThreatScanInterval)
	{
		InstanceData.TimeSinceThreatScan = 0.f;

		bool bThreatNear = false;
		bool bHostEngaged = false;
		if (AAIController* AIController = Context.GetExternalDataPtr(AIControllerHandle))
		{
			if (const IGenericTeamAgentInterface* TeamAgent = Cast<IGenericTeamAgentInterface>(AIController))
			{
				if (World)
				{
					const FVector Origin = Pawn->GetActorLocation();
					const float ThreatRadiusSq = FMath::Square(ThreatNearRadius);
					const float HostRadiusSq = FMath::Square(HostCombatRadius);
					for (TActorIterator<APawn> It(World); It; ++It)
					{
						APawn* Other = *It;
						if (!Other || Other == Pawn)
						{
							continue;
						}
						if (TeamAgent->GetTeamAttitudeTowards(*Other) != ETeamAttitude::Hostile)
						{
							continue;
						}
						if (!bThreatNear && FVector::DistSquared(Origin, Other->GetActorLocation()) <= ThreatRadiusSq)
						{
							bThreatNear = true;
						}
						if (!bHostEngaged && Host
							&& FVector::DistSquared(Host->GetActorLocation(), Other->GetActorLocation()) <= HostRadiusSq)
						{
							if (const IAbilitySystemInterface* EnemyASI = Cast<IAbilitySystemInterface>(Other))
							{
								if (UAbilitySystemComponent* EnemyASC = EnemyASI->GetAbilitySystemComponent())
								{
									bHostEngaged =
										!EnemyASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Enemy_Dead)
										&& !EnemyASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Enemy_Idle)
										&& !EnemyASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Enemy_Return);
								}
							}
						}
						if (bThreatNear && bHostEngaged)
						{
							break;
						}
					}
				}
			}
		}
		InstanceData.bThreatNear = bThreatNear;
		InstanceData.bHostEngaged = bHostEngaged;
	}
}
