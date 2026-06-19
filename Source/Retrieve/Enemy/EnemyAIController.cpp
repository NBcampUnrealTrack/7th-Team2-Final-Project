#include "Enemy/EnemyAIController.h"

#include "Components/StateTreeAIComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISenseConfig_Sight.h"
#include "GenericTeamAgentInterface.h"

AEnemyAIController::AEnemyAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	StateTreeAIComp = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeAIComponent"));
	StateTreeAIComp->SetStartLogicAutomatically(false);

	AIPerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerceptionComponent"));
	SetPerceptionComponent(*AIPerceptionComp);
	
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	DamageConfig = CreateDefaultSubobject<UAISenseConfig_Damage>(TEXT("DamageConfig"));
	
	SetGenericTeamId(FGenericTeamId(static_cast<uint8>(Team)));
	
	bAllowStrafe = true;
}

void AEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	
	GetWorld()->GetTimerManager().SetTimerForNextTick(
			this, &AEnemyAIController::RestartStateTree);
}

void AEnemyAIController::OnUnPossess()
{
	Super::OnUnPossess();
}

void AEnemyAIController::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	
	if (!SightConfig || !DamageConfig || !AIPerceptionComp)
	{
		return;
	}
	
	InitSightConfig();
	InitDamageConfig();
}

void AEnemyAIController::ConfigureStateTree(UStateTree* InStateTree)
{
	if (StateTreeAIComp && InStateTree)
	{
		DefaultStateTree = InStateTree;
		StateTreeAIComp->SetStartLogicAutomatically(false);
		StateTreeAIComp->SetStateTree(DefaultStateTree);
		StateTreeAIComp->StartLogic();
	}
}

ETeamAttitude::Type AEnemyAIController::GetTeamAttitudeTowards(const AActor& Other) const
{
	ETeamAttitude::Type Result = ETeamAttitude::Neutral;

	if (const APawn* OtherPawn = Cast<APawn>(&Other))
	{
		FGenericTeamId OtherTeamId = FGenericTeamId::NoTeam;
		if (const IGenericTeamAgentInterface* CtrlTeam = Cast<IGenericTeamAgentInterface>(OtherPawn->GetController()))
		{
			OtherTeamId = CtrlTeam->GetGenericTeamId();
		}
		
		if (OtherTeamId == FGenericTeamId::NoTeam)   // 컨트롤러 팀 없으면 폰 팀 사용
		{
			if (const IGenericTeamAgentInterface* PawnTeam = Cast<IGenericTeamAgentInterface>(OtherPawn))
			{
				OtherTeamId = PawnTeam->GetGenericTeamId();
			}
		}

		// 컨트롤러·폰 모두 팀 인터페이스가 없으면 Neutral 처리
		// (기본 solver는 NoTeam(255) != Enemy(2) = Hostile로 판정하므로 명시적으로 차단)
		if (OtherTeamId == FGenericTeamId::NoTeam)
		{
			return ETeamAttitude::Neutral;
		}

		Result = FGenericTeamId::GetAttitude(GetGenericTeamId(), OtherTeamId);
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

void AEnemyAIController::RestartStateTree()
{
	if (StateTreeAIComp && StateTreeAIComp->IsRunning())
	{
		StateTreeAIComp->StopLogic("RestartOnPossess");
	}

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

void AEnemyAIController::InitDamageConfig()
{
	DamageConfig->SetMaxAge(5.0f);

	AIPerceptionComp->ConfigureSense(*DamageConfig);
	AIPerceptionComp->RequestStimuliListenerUpdate();
}

void AEnemyAIController::TryStartStateTree()
{
	if (StateTreeAIComp && !StateTreeAIComp->IsRunning())
	{
		StateTreeAIComp->SetStartLogicAutomatically(false);
		if (DefaultStateTree)
		{
			StateTreeAIComp->SetStateTree(DefaultStateTree);
		}
		StateTreeAIComp->StartLogic();
	}
}
