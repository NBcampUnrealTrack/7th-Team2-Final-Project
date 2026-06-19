#include "Lumen/LumenAIController.h"

#include "Components/StateTreeAIComponent.h"

ALumenAIController::ALumenAIController(const FObjectInitializer& ObjectInitializer)
{
	StateTreeAIComp = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeAIComponent"));
	SetGenericTeamId(FGenericTeamId(static_cast<uint8>(Team)));
}

void ALumenAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ALumenAIController::TryStartStateTree);
}

void ALumenAIController::TryStartStateTree()
{
	if (StateTreeAIComp && !StateTreeAIComp->IsRunning())
	{
		StateTreeAIComp->StartLogic();
	}
}

ETeamAttitude::Type ALumenAIController::GetTeamAttitudeTowards(const AActor& Other) const
{
	ETeamAttitude::Type Result = ETeamAttitude::Neutral;

	if (const APawn* OtherPawn = Cast<APawn>(&Other))
	{
		FGenericTeamId OtherTeamId = FGenericTeamId::NoTeam;
		if (const IGenericTeamAgentInterface* CtrlTeam = Cast<IGenericTeamAgentInterface>(OtherPawn->GetController()))
		{
			OtherTeamId = CtrlTeam->GetGenericTeamId();
		}
		if (OtherTeamId == FGenericTeamId::NoTeam)
		{
			if (const IGenericTeamAgentInterface* PawnTeam = Cast<IGenericTeamAgentInterface>(OtherPawn))
			{
				OtherTeamId = PawnTeam->GetGenericTeamId();
			}
		}
		if (OtherTeamId == FGenericTeamId::NoTeam)
		{
			return ETeamAttitude::Neutral;
		}
		Result = FGenericTeamId::GetAttitude(GetGenericTeamId(), OtherTeamId);
	}

	return Result;
}
