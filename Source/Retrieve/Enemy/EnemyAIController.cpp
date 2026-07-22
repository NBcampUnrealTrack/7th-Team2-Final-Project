#include "Enemy/EnemyAIController.h"

#include "Components/StateTreeAIComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISenseConfig_Sight.h"
#include "GenericTeamAgentInterface.h"
#include "Character/RetrieveEnemyCharacter.h"
#include "Character/LumenCharacter.h"
#include "StateTree.h"

namespace
{
	constexpr TCHAR EpicMonsterStateTreePath[] =
		TEXT("/Game/Retrieve/AI/StateTrees/ST_Monster_Epic.ST_Monster_Epic");
}

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
	InitSightConfig();
	
	GetWorld()->GetTimerManager().SetTimerForNextTick(
			this, &AEnemyAIController::RestartStateTree);
}

void AEnemyAIController::OnUnPossess()
{
	RecognizedTarget.Reset();
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
	// 루멘은 플레이어의 동반 NPC로, 몬스터의 인식·공격 대상에서 무조건 제외한다.
	// (LumenAIController가 세팅한 Team ID가 Enemy와 다른 값이라 기본 solver로는 Hostile 판정되므로 여기서 명시적 차단)
	if (Cast<const ALumenCharacter>(&Other))
	{
		return ETeamAttitude::Neutral;
	}

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
	RecognizedTarget.Reset();

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
	if (!SightConfig || !AIPerceptionComp)
	{
		return;
	}

	SightConfig->SightRadius = GetEffectiveSightRadius();
	SightConfig->LoseSightRadius = GetEffectiveLoseSightRadius();
	SightConfig->PeripheralVisionAngleDegrees = GetEffectivePeripheralVisionAngleDegrees();
	SightConfig->SetMaxAge(5.0f);
	
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = false;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;

	AIPerceptionComp->ConfigureSense(*SightConfig);
	AIPerceptionComp->SetDominantSense(SightConfig->GetSenseImplementation());
	AIPerceptionComp->RequestStimuliListenerUpdate();
}

float AEnemyAIController::GetEffectiveSightRadius() const
{
	const ARetrieveEnemyCharacter* Enemy = Cast<ARetrieveEnemyCharacter>(GetPawn());
	if (const FMonsterDataRow* Row = Enemy ? Enemy->GetMonsterDataRow() : nullptr)
	{
		return Row->SightRadius;
	}
	return SightRadius;
}

float AEnemyAIController::GetEffectiveLoseSightRadius() const
{
	const ARetrieveEnemyCharacter* Enemy = Cast<ARetrieveEnemyCharacter>(GetPawn());
	if (const FMonsterDataRow* Row = Enemy ? Enemy->GetMonsterDataRow() : nullptr)
	{
		return Row->LoseSightRadius;
	}
	return LoseSightRadius;
}

void AEnemyAIController::SetRecognizedTarget(AActor* InTarget)
{
	RecognizedTarget = InTarget;
}

bool AEnemyAIController::IsRecognizingTarget(const AActor* Target) const
{
	return IsValid(Target) && RecognizedTarget.Get() == Target;
}

float AEnemyAIController::GetEffectivePeripheralVisionAngleDegrees() const
{
	const ARetrieveEnemyCharacter* Enemy = Cast<ARetrieveEnemyCharacter>(GetPawn());
	const float OverrideAngle = Enemy ? Enemy->GetPeripheralVisionAngleOverrideForAI() : -1.f;
	return OverrideAngle > 0.f
		? FMath::Max(PeripheralVisionAngleDegrees, OverrideAngle)
		: PeripheralVisionAngleDegrees;
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
		const ARetrieveEnemyCharacter* Enemy = Cast<ARetrieveEnemyCharacter>(GetPawn());
		if (!DefaultStateTree && Enemy && Enemy->ShouldUseFallbackEpicStateTree())
		{
			DefaultStateTree = Cast<UStateTree>(StaticLoadObject(
				UStateTree::StaticClass(),
				nullptr,
				EpicMonsterStateTreePath));
		}

		if (DefaultStateTree)
		{
			StateTreeAIComp->SetStateTree(DefaultStateTree);
		}

		// StartLogic은 항상 호출해야 한다.
		// 일반/보스 컨트롤러는 DefaultStateTree 멤버를 비워 두고 StateTreeAIComponent에
		// 직접 설정된 StateTree 에셋으로 동작한다. StartLogic을 if(DefaultStateTree) 블록
		// 안에서만 호출하면 이 경우 로직이 시작되지 않아 몬스터가 완전히 정지한다. (회귀 수정)
		StateTreeAIComp->StartLogic();
	}
}
