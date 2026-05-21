#include "Enemy/EnemyAIController.h"

#include "Components/StateTreeAIComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "StructUtils/InstancedStruct.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Character/SovereignCharacter.h"
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

	AIPerceptionComp->OnTargetPerceptionUpdated.AddDynamic(
		this, &AEnemyAIController::OnTargetPerceptionUpdated);
}

void AEnemyAIController::OnUnPossess()
{
	Super::OnUnPossess();
	
	if (IsValid(AIPerceptionComp))
	{
		AIPerceptionComp->OnTargetPerceptionUpdated.RemoveDynamic(this, &AEnemyAIController::OnTargetPerceptionUpdated);
	}
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

void AEnemyAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Actor) return;
	
	if (Stimulus.WasSuccessfullySensed())
	{
		if (Cast<ASovereignCharacter>(Actor))
		{
			UGameplayMessageSubsystem& MsgSubsys = UGameplayMessageSubsystem::Get(GetWorld());
		
			FGameplayTag Channel = RetrieveGameplayTags::Channel_Enemy_PlayerSpotted;
			MsgSubsys.BroadcastMessage(Channel, FInstancedStruct());
		}
	}
}

void AEnemyAIController::InitSightConfig()
{
	SightConfig->SightRadius = SightRadius;
	SightConfig->LoseSightRadius = LoseSightRadius;
	SightConfig->PeripheralVisionAngleDegrees = PeripheralVisionAngleDegrees;
	SightConfig->SetMaxAge(MaxAge);
	
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = false;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;

	AIPerceptionComp->ConfigureSense(*SightConfig);
	AIPerceptionComp->SetDominantSense(SightConfig->GetSenseImplementation());
	AIPerceptionComp->RequestStimuliListenerUpdate();
}
