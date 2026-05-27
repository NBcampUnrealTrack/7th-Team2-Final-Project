#include "Enemy/EnemyAIController.h"

#include "Components/StateTreeAIComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "GenericTeamAgentInterface.h"

AEnemyAIController::AEnemyAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	StateTreeAIComp = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeAIComponent"));

	AIPerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerceptionComponent"));
	SetPerceptionComponent(*AIPerceptionComp);
	
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	
	SetGenericTeamId(FGenericTeamId(EnemyTeamId));
}

void AEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	
	GetWorld()->GetTimerManager().SetTimerForNextTick(
			this, &AEnemyAIController::TryStartStateTree);
}

void AEnemyAIController::OnUnPossess()
{
	Super::OnUnPossess();
}

void AEnemyAIController::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	
	if (!SightConfig || !AIPerceptionComp)
	{
		return;
	}
	
	InitSightConfig();
}

void AEnemyAIController::ConfigureStateTree(UStateTree* InStateTree) const
{
	if (StateTreeAIComp && InStateTree)
	{
        StateTreeAIComp->SetStartLogicAutomatically(false);
		StateTreeAIComp->SetStateTree(InStateTree);
        StateTreeAIComp->StartLogic();   
	}
}

ETeamAttitude::Type AEnemyAIController::GetTeamAttitudeTowards(const AActor& Other) const
{
	ETeamAttitude::Type Result = ETeamAttitude::Neutral;

	if (const APawn* OtherPawn = Cast<APawn>(&Other))
	{
		if (const IGenericTeamAgentInterface* TeamAgent =
			Cast<IGenericTeamAgentInterface>(OtherPawn->GetController()))
		{
			Result = FGenericTeamId::GetAttitude(GetGenericTeamId(), TeamAgent->GetGenericTeamId());
		}
		else if (const IGenericTeamAgentInterface* PawnTeam = Cast<IGenericTeamAgentInterface>(OtherPawn))
		{
			Result = FGenericTeamId::GetAttitude(GetGenericTeamId(), PawnTeam->GetGenericTeamId());
		}
	}
	
	return Result;
}

void AEnemyAIController::Deactivate()
{
	if (StateTreeAIComp && StateTreeAIComp->IsRunning())
	{
		StateTreeAIComp->StopLogic("Deactivated");
	}
	
	if (AIPerceptionComp)
	{
		AIPerceptionComp->ForgetAll();
	}
}

void AEnemyAIController::Reactivate()
{
	TryStartStateTree();
}

void AEnemyAIController::InitSightConfig()
{
	SightConfig->SightRadius = SightRadius;
	SightConfig->LoseSightRadius = LoseSightRadius;
	SightConfig->PeripheralVisionAngleDegrees = PeripheralVisionAngleDegrees;
	SightConfig->SetMaxAge(5.0f);
	
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = false;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;

	AIPerceptionComp->ConfigureSense(*SightConfig);
	AIPerceptionComp->SetDominantSense(SightConfig->GetSenseImplementation());
	AIPerceptionComp->RequestStimuliListenerUpdate();
}

void AEnemyAIController::TryStartStateTree()
{
	if (StateTreeAIComp && !StateTreeAIComp->IsRunning())
	{
		StateTreeAIComp->StartLogic();
	}
}
