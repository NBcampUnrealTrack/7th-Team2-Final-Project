#include "NPC/NPCPatrolAIController.h"

#include "Components/StateTreeAIComponent.h"
#include "StateTree.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
	// BP 서브클래스에서 DefaultStateTree를 별도로 지정하지 않았을 때 사용하는 기본 순찰 StateTree.
	constexpr TCHAR DefaultVillagerPatrolStateTreePath[] =
		TEXT("/Game/Retrieve/AI/StateTrees/ST_NPC_VillagerPatrol.ST_NPC_VillagerPatrol");
}

ANPCPatrolAIController::ANPCPatrolAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	StateTreeAIComp = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeAIComponent"));
	StateTreeAIComp->SetStartLogicAutomatically(false);

	bAllowStrafe = false;
}

void ANPCPatrolAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// Pawn 초기화가 끝난 다음 틱에 시작해 ST 컴포넌트의 컨텍스트 요구사항 누락을 피한다.
	GetWorld()->GetTimerManager().SetTimerForNextTick(
		this, &ANPCPatrolAIController::TryStartStateTree);
}

void ANPCPatrolAIController::Deactivate()
{
	if (StateTreeAIComp && StateTreeAIComp->IsRunning())
	{
		StateTreeAIComp->StopLogic("Deactivated");
	}
	StopMovement();

	// StateTree가 멈추면 FStateTreeTask_VillagerPatrol::Tick()의 피치/롤 강제 보정도 함께 멈춘다.
	// 대화 등으로 비활성화되는 순간 이동 관성(가속도/속도)이 남아있으면, 그 보정이 다시는 걸리지 않아
	// 대화가 끝날 때까지 캐릭터가 기울어진 채로 굳어버릴 수 있다. 여기서 즉시 정리한다.
	if (APawn* ControlledPawn = GetPawn())
	{
		if (ACharacter* ControlledCharacter = Cast<ACharacter>(ControlledPawn))
		{
			if (UCharacterMovementComponent* MoveComp = ControlledCharacter->GetCharacterMovement())
			{
				MoveComp->StopMovementImmediately();
			}
		}

		const FRotator CurrentRotation = ControlledPawn->GetActorRotation();
		if (!FMath::IsNearlyZero(CurrentRotation.Pitch) || !FMath::IsNearlyZero(CurrentRotation.Roll))
		{
			ControlledPawn->SetActorRotation(FRotator(0.f, CurrentRotation.Yaw, 0.f));
		}
	}
}

void ANPCPatrolAIController::Reactivate()
{
	TryStartStateTree();
}

void ANPCPatrolAIController::TryStartStateTree()
{
	if (StateTreeAIComp && !StateTreeAIComp->IsRunning())
	{
		if (!DefaultStateTree)
		{
			DefaultStateTree = Cast<UStateTree>(StaticLoadObject(
				UStateTree::StaticClass(),
				nullptr,
				DefaultVillagerPatrolStateTreePath));
		}

		if (DefaultStateTree)
		{
			StateTreeAIComp->SetStartLogicAutomatically(false);
			StateTreeAIComp->SetStateTree(DefaultStateTree);
			StateTreeAIComp->StartLogic();
		}
	}
}
