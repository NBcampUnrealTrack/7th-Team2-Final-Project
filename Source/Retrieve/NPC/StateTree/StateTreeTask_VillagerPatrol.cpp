#include "NPC/StateTree/StateTreeTask_VillagerPatrol.h"

#include "StateTreeLinker.h"
#include "StateTreeExecutionContext.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Character/RetrieveVillagerCharacter.h"
#include "NPC/NPCPatrolZone.h"
#include "NPC/NPCPatrolCoordinatorSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimMontage.h"

bool FStateTreeTask_VillagerPatrol::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(PawnHandle);
	return true;
}

bool FStateTreeTask_VillagerPatrol::PickAndMoveToNewPoint(FInstanceDataType& InstanceData, APawn* Pawn) const
{
	if (!Pawn)
	{
		return false;
	}

	AAIController* AIC = Pawn->GetController<AAIController>();
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Pawn->GetWorld());
	if (!AIC || !NavSys)
	{
		return false;
	}

	const ARetrieveVillagerCharacter* Villager = Cast<ARetrieveVillagerCharacter>(Pawn);
	const float PatrolRadius = Villager ? Villager->PatrolRadius : 800.f;
	const float AcceptanceRadius = Villager ? Villager->PatrolAcceptanceRadius : 120.f;
	const float MinNeighborSeparation = Villager ? Villager->PatrolMinNeighborSeparation : 250.f;
	ANPCPatrolZone* Zone = Villager ? Villager->PatrolZone : nullptr;

	// 탐색 기준점은 항상 NPC 자신의 스폰 위치를 쓴다(캐릭터가 실제로 서 있던 곳이라 내비메시 투영이 보장됨).
	// 구역 볼륨의 피벗은 디자이너가 지면에 정확히 맞춰 배치했다는 보장이 없어, 그 위치를 기준점으로 쓰면
	// 내비메시 투영에 실패해 GetRandomReachablePointInRadius가 항상 실패하고 NPC가 멈춰버릴 수 있다.
	// 대신 반경만 "스폰 위치→구역 중심 거리 + 구역 반경"으로 넉넉히 잡아 구역 전체를 커버한다.
	const FVector SearchOrigin = InstanceData.PatrolOrigin;
	const float SearchRadius = Zone
		? FVector::Dist(InstanceData.PatrolOrigin, Zone->GetZoneCenter()) + Zone->GetZoneRadius()
		: PatrolRadius;

	UNPCPatrolCoordinatorSubsystem* Coordinator = Pawn->GetWorld()->GetSubsystem<UNPCPatrolCoordinatorSubsystem>();

	// 구역 경계/이웃 NPC 목적지와의 최소 간격을 만족하는 지점을 찾을 때까지 몇 차례 재시도한다.
	// 구역 필터링은 확률적으로 걸러지므로(넓은 원에서 좁은 구역만 채택) 시도 횟수를 넉넉히 둔다.
	const int32 MaxAttempts = Zone ? 20 : 8;
	FNavLocation RandomPoint;
	bool bFoundPoint = false;
	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		if (!NavSys->GetRandomReachablePointInRadius(SearchOrigin, SearchRadius, RandomPoint))
		{
			continue;
		}
		if (Zone && !Zone->ContainsPoint(RandomPoint.Location))
		{
			continue;
		}
		if (Coordinator && Coordinator->IsPointTooCrowded(RandomPoint.Location, AIC, MinNeighborSeparation))
		{
			continue;
		}
		bFoundPoint = true;
		break;
	}

	if (!bFoundPoint)
	{
		return false;
	}

	InstanceData.CurrentPatrolPoint = RandomPoint.Location;
	InstanceData.bHasPoint = true;
	InstanceData.bWaiting = false;
	InstanceData.MoveElapsed = 0.f;

	if (Coordinator)
	{
		Coordinator->ReserveDestination(AIC, InstanceData.CurrentPatrolPoint);
	}

	// CharacterMovement->bOrientRotationToMovement가 회전을 전담하므로 bCanStrafe=true로 넘겨
	// PathFollowingComponent가 별도로 회전을 통제하지 않게 한다 (충돌 시 미끄러지는 이동 발생).
	AIC->MoveToLocation(
		InstanceData.CurrentPatrolPoint,
		AcceptanceRadius,
		/*bStopOnOverlap=*/true,
		/*bUsePathfinding=*/true,
		/*bProjectDestinationToNavigation=*/true,
		/*bCanStrafe=*/true);

	return true;
}

void FStateTreeTask_VillagerPatrol::TryPlayIdleAction(const ARetrieveVillagerCharacter* Villager, APawn* Pawn,
                                                       FInstanceDataType& InstanceData) const
{
	if (!Villager || Villager->IdleActionMontages.Num() == 0 || FMath::FRand() > Villager->IdleActionChance)
	{
		return;
	}

	const int32 ChosenIndex = FMath::RandRange(0, Villager->IdleActionMontages.Num() - 1);
	if (UAnimMontage* ChosenMontage = Villager->IdleActionMontages[ChosenIndex])
	{
		if (ACharacter* Character = Cast<ACharacter>(Pawn))
		{
			const float MontageLength = Character->PlayAnimMontage(ChosenMontage);
			InstanceData.WaitTimer = FMath::Max(InstanceData.WaitTimer, MontageLength);
		}
	}
}

EStateTreeRunStatus FStateTreeTask_VillagerPatrol::EnterState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);
	if (!Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (!InstanceData.bOriginCaptured)
	{
		const ARetrieveVillagerCharacter* Villager = Cast<ARetrieveVillagerCharacter>(Pawn);
		InstanceData.PatrolOrigin = Villager
			? Villager->GetInitialSpawnLocation()
			: Pawn->GetActorLocation();
		InstanceData.bOriginCaptured = true;
	}

	InstanceData.bHasPoint = false;
	InstanceData.bWaiting = false;
	InstanceData.WaitTimer = 0.f;
	InstanceData.MoveElapsed = 0.f;
	InstanceData.bGreeting = false;
	InstanceData.GreetTimer = 0.f;
	InstanceData.GreetCooldownRemaining = 0.f;

	if (AAIController* AIC = Pawn->GetController<AAIController>())
	{
		if (UNPCPatrolCoordinatorSubsystem* Coordinator = Pawn->GetWorld()->GetSubsystem<UNPCPatrolCoordinatorSubsystem>())
		{
			Coordinator->RegisterActivePatroller(AIC);
		}
	}

	// 첫 지점 선정 실패 시(네비게이션 미준비 등) 짧게 대기 후 Tick에서 재시도
	if (!PickAndMoveToNewPoint(InstanceData, Pawn))
	{
		InstanceData.bWaiting = true;
		InstanceData.WaitTimer = 0.5f;
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_VillagerPatrol::Tick(
	FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);
	if (!Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}

	const ARetrieveVillagerCharacter* Villager = Cast<ARetrieveVillagerCharacter>(Pawn);
	const float WaitTimeMin = Villager ? Villager->PatrolWaitTimeMin : 2.f;
	const float WaitTimeMax = Villager ? Villager->PatrolWaitTimeMax : 5.f;
	const float AcceptanceRadius = Villager ? Villager->PatrolAcceptanceRadius : 120.f;
	const float MaxMoveTime = Villager ? Villager->PatrolMaxMoveTime : 8.f;
	const float GreetCooldown = Villager ? Villager->GreetCooldown : 15.f;

	if (InstanceData.GreetCooldownRemaining > 0.f)
	{
		InstanceData.GreetCooldownRemaining -= DeltaTime;
	}

	// 다른 NPC와 마주쳐 대화 중이면 그 처리만 하고 나머지 순찰 로직은 건너뛴다.
	if (InstanceData.bGreeting)
	{
		InstanceData.GreetTimer -= DeltaTime;
		if (InstanceData.GreetTimer <= 0.f)
		{
			InstanceData.bGreeting = false;
			InstanceData.GreetCooldownRemaining = GreetCooldown;
			InstanceData.bWaiting = true;
			InstanceData.WaitTimer = FMath::FRandRange(WaitTimeMin, FMath::Max(WaitTimeMin, WaitTimeMax));
			TryPlayIdleAction(Villager, Pawn, InstanceData);
		}
		return EStateTreeRunStatus::Running;
	}

	// 이동/대기 중이든 상관없이, 쿨다운이 끝났고 다른 순찰 NPC가 가까워지면 서로 마주보고 대화한다.
	if (InstanceData.GreetCooldownRemaining <= 0.f)
	{
		if (AAIController* AIC = Pawn->GetController<AAIController>())
		{
			if (UNPCPatrolCoordinatorSubsystem* Coordinator = Pawn->GetWorld()->GetSubsystem<UNPCPatrolCoordinatorSubsystem>())
			{
				const float GreetTriggerRadius = Villager ? Villager->GreetTriggerRadius : 200.f;
				if (AAIController* Partner = Coordinator->FindNearbyPatroller(Pawn->GetActorLocation(), GreetTriggerRadius, AIC))
				{
					if (APawn* PartnerPawn = Partner->GetPawn())
					{
						AIC->StopMovement();

						// StopMovement()는 경로 추종만 끊을 뿐 관성(브레이킹 감속)으로 속도가 서서히 0이 되므로,
						// 완전정지 포즈의 인사 몽타주가 블렌드인되는 동안 다리가 아직 미끄러지는 상태와 겹쳐
						// 순간적으로 자세가 무너져 보일 수 있다. 인사 시작 시 속도를 즉시 0으로 만든다.
						if (ACharacter* StoppingCharacter = Cast<ACharacter>(Pawn))
						{
							if (UCharacterMovementComponent* MoveComp = StoppingCharacter->GetCharacterMovement())
							{
								MoveComp->StopMovementImmediately();
							}
						}

						// Z(높이차)를 포함해 회전시키면 지형 높이차가 있을 때 캐릭터가 앞/뒤로 크게 기울어(누운 것처럼) 보이므로
						// 요(yaw)만 반영해 수평으로만 마주보게 한다.
						FVector ToPartner = PartnerPawn->GetActorLocation() - Pawn->GetActorLocation();
						ToPartner.Z = 0.f;
						if (!ToPartner.IsNearlyZero())
						{
							Pawn->SetActorRotation(ToPartner.Rotation());
						}

						float Duration = Villager ? Villager->GreetDuration : 3.f;
						if (Villager && Villager->GreetMontage)
						{
							if (ACharacter* Character = Cast<ACharacter>(Pawn))
							{
								const float MontageLength = Character->PlayAnimMontage(Villager->GreetMontage);
								Duration = FMath::Max(Duration, MontageLength);
							}
						}

						InstanceData.bGreeting = true;
						InstanceData.bWaiting = false;
						InstanceData.GreetTimer = Duration;
						return EStateTreeRunStatus::Running;
					}
				}
			}
		}
	}

	// 지점 도착 후 대기 단계
	if (InstanceData.bWaiting)
	{
		InstanceData.WaitTimer -= DeltaTime;
		if (InstanceData.WaitTimer <= 0.f)
		{
			if (!PickAndMoveToNewPoint(InstanceData, Pawn))
			{
				InstanceData.bWaiting = true;
				InstanceData.WaitTimer = 0.5f;
			}
		}
		return EStateTreeRunStatus::Running;
	}

	// 이동 단계
	InstanceData.MoveElapsed += DeltaTime;

	const float DistToPoint = FVector::Dist2D(Pawn->GetActorLocation(), InstanceData.CurrentPatrolPoint);

	bool bArrivedOrStuck = false;
	if (!InstanceData.bHasPoint)
	{
		bArrivedOrStuck = true;
	}
	else if (DistToPoint <= AcceptanceRadius)
	{
		bArrivedOrStuck = true; // 도착
	}
	else if (InstanceData.MoveElapsed >= MaxMoveTime)
	{
		bArrivedOrStuck = true; // 막힘 → 재선정
	}
	else if (InstanceData.MoveElapsed >= 0.3f)
	{
		// 이동 요청 직후 한 틱의 Idle 오판을 피하기 위한 최소 이동 시간 이후에만 상태를 신뢰
		if (AAIController* AIC = Pawn->GetController<AAIController>())
		{
			if (AIC->GetMoveStatus() == EPathFollowingStatus::Idle)
			{
				bArrivedOrStuck = true;
			}
		}
	}

	if (bArrivedOrStuck)
	{
		if (AAIController* AIC = Pawn->GetController<AAIController>())
		{
			AIC->StopMovement();
		}
		InstanceData.bWaiting = true;
		InstanceData.WaitTimer = FMath::FRandRange(WaitTimeMin, FMath::Max(WaitTimeMin, WaitTimeMax));
		TryPlayIdleAction(Villager, Pawn, InstanceData);
	}

	return EStateTreeRunStatus::Running;
}

void FStateTreeTask_VillagerPatrol::ExitState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);
	if (!Pawn)
	{
		return;
	}

	if (AAIController* AIC = Pawn->GetController<AAIController>())
	{
		AIC->StopMovement();
		if (UNPCPatrolCoordinatorSubsystem* Coordinator = Pawn->GetWorld()->GetSubsystem<UNPCPatrolCoordinatorSubsystem>())
		{
			Coordinator->ReleaseDestination(AIC);
			Coordinator->UnregisterActivePatroller(AIC);
		}
	}
}
