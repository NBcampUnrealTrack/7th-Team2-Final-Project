#include "Enemy/StateTree/RetrieveEnemyTargetEvaluator.h"

#include "StateTreeLinker.h"
#include "StateTreeExecutionContext.h"
#include "AIController.h"
#include "Enemy/EnemyAIController.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense.h"
#include "Perception/AISense_Damage.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Messaging/GameplayMessages/RetrieveGameplayMessageTypes.h"
#include "Character/RetrieveEnemyCharacter.h"
#include "NavigationSystem.h"
#include "Components/Combat/RetrieveHealthComponent.h"
#include "Enemy/EncirclementSubsystem.h"
#include "Components/Enemy/EnemyCombatComponent.h"
#include "Components/Enemy/EnemySuspicionIndicatorComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Logging/RetrieveLogChannels.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Navigation/PathFollowingComponent.h"

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

	bool WasDamageSensed(UAIPerceptionComponent* PerceptionComp, AActor* Actor)
	{
		if (!PerceptionComp || !IsValid(Actor))
		{
			return false;
		}

		FActorPerceptionBlueprintInfo PerceptionInfo;
		if (!PerceptionComp->GetActorsPerception(Actor, PerceptionInfo))
		{
			return false;
		}

		const FAISenseID DamageSenseID = UAISense::GetSenseID<UAISense_Damage>();
		for (const FAIStimulus& Stimulus : PerceptionInfo.LastSensedStimuli)
		{
			if (Stimulus.Type == DamageSenseID && Stimulus.WasSuccessfullySensed())
			{
				return true;
			}
		}

		return false;
	}

	bool ContainsActor(const TArray<AActor*>& Actors, const AActor* Actor)
	{
		return Actor && Actors.ContainsByPredicate([Actor](const AActor* Candidate)
		{
			return Candidate == Actor;
		});
	}

	// 시네마틱 재생 중(State.Player.Cinematic 태그)인 액터는 타겟팅에서 제외 — 연출 중 어그로/공격 방지.
	bool IsCinematicSuppressedActor(AActor* Actor)
	{
		const IAbilitySystemInterface* ASCInterface = Cast<IAbilitySystemInterface>(Actor);
		UAbilitySystemComponent* ASC = ASCInterface ? ASCInterface->GetAbilitySystemComponent() : nullptr;
		return ASC && ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Cinematic);
	}

	// 타겟을 최종 해제한다. 슬롯/토큰 반납 + Perception known 제거 + InstanceData 코어 정리를 원자적으로 수행.
	// ForgetActor를 함께 호출하지 않으면 AISense_Sight의 MaxAge 동안 known에 잔존해 다음 틱에 재획득된다.
	void ClearTarget(FRetrieveEnemyTargetEvalInstanceData& InstanceData,
		UAIPerceptionComponent* PerceptionComp,
		UEncirclementSubsystem* EncirclementSubsystem,
		APawn* Pawn)
	{
		if (AEnemyAIController* EnemyAIController = Pawn ? Cast<AEnemyAIController>(Pawn->GetController()) : nullptr)
		{
			EnemyAIController->SetRecognizedTarget(nullptr);
		}

		if (!IsValid(InstanceData.TargetPlayer))
		{
			InstanceData.TargetPlayer = nullptr;
			InstanceData.DistanceToTarget = 0.f;
			InstanceData.bTargetLost = true;
			return;
		}
		if (EncirclementSubsystem)
		{
			EncirclementSubsystem->ReleaseSlot(InstanceData.TargetPlayer, Pawn);
			EncirclementSubsystem->ReleaseAttackToken(InstanceData.TargetPlayer, Pawn);
		}
		if (PerceptionComp)
		{
			PerceptionComp->ForgetActor(InstanceData.TargetPlayer);
		}
		InstanceData.TargetPlayer = nullptr;
		InstanceData.DistanceToTarget = 0.f;
		InstanceData.bTargetLost = true;
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

	if (AEnemyAIController* EnemyAIController = Cast<AEnemyAIController>(
		Context.GetExternalDataPtr(AIControllerHandle)))
	{
		EnemyAIController->SetRecognizedTarget(nullptr);
	}
	
	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);
	if (Pawn)
	{
		InstanceData.SpawnedLocation = Pawn->GetActorLocation();
		
		if (UEnemyCombatComponent* CombatComp = Pawn->GetComponentByClass<UEnemyCombatComponent>())
		{
			InstanceData.CachedCombatComponent = CombatComp;
		}

		if (UEnemySuspicionIndicatorComponent* SuspicionIndicator = Pawn->GetComponentByClass<UEnemySuspicionIndicatorComponent>())
		{
			InstanceData.CachedSuspicionIndicator = SuspicionIndicator;
		}
	}
	
	InstanceData.bOutOfChaseRange = false;
	
	if (ARetrieveEnemyCharacter* Enemy = Cast<ARetrieveEnemyCharacter>(Pawn))
	{
		InstanceData.bUseDirectChaseToTarget = Enemy->ShouldUseDirectChaseToTarget();
	
		if (const FMonsterDataRow* Row = Enemy->GetMonsterDataRow())
		{
			InstanceData.AttackableRange = Row->AttackableRange;
			InstanceData.StrafeOffRange = Row->StrafeOffRange;
			InstanceData.StrafeMinNoise = Row->StrafeMinNoise;
			InstanceData.StrafeMaxNoise = Row->StrafeMaxNoise;
			InstanceData.OrbitInnerRadius = Row->OrbitInnerRadius;
			InstanceData.OrbitOuterRadius = Row->OrbitOuterRadius;
			InstanceData.ChaseRange = Row->ChaseRange;
			InstanceData.RechasableRange = Row->RechasableRange;
			InstanceData.SuspicionIncreaseRate = Row->SuspicionIncreaseRate;
			InstanceData.SuspicionDecreaseRate = Row->SuspicionDecreaseRate;
			InstanceData.ForceCombatRange = Row->ForceCombatRange;
			InstanceData.bPatrolable = Row->bPatrolable;
			InstanceData.PatrolRange = Row->PatrolRange;
			InstanceData.MoveAcceptableRadius = Row->MoveAcceptableRadius;
			if (Row->bOverrideDirectChaseToTarget)
			{
				InstanceData.bUseDirectChaseToTarget = Row->bUseDirectChaseToTarget;
			}
			InstanceData.bHasAerialPhase = Enemy->ShouldUseStateTreeAerialPhase()
				&& !Enemy->IsAerialSpecialAttackReady();
		}
	}
}

void FRetrieveEnemyTargetEvaluator::TreeStop(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);
	
	if (Pawn && InstanceData.TargetPlayer)
	{
		if (UEncirclementSubsystem* EncirclementSubsystem = Pawn->GetWorld()->GetSubsystem<UEncirclementSubsystem>())
		{
			EncirclementSubsystem->ReleaseSlot(InstanceData.TargetPlayer, Pawn);
			EncirclementSubsystem->ReleaseAttackToken(InstanceData.TargetPlayer, Pawn);
		}
	}

	if (AEnemyAIController* EnemyAIController = Cast<AEnemyAIController>(
		Context.GetExternalDataPtr(AIControllerHandle)))
	{
		EnemyAIController->SetRecognizedTarget(nullptr);
	}
}

void FRetrieveEnemyTargetEvaluator::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// 위치 갱신은 매 틱 진행하도록 변경
	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);
	if (Pawn && IsValid(InstanceData.TargetPlayer))
	{
		// 시네마틱이 시작된 타겟은 사망과 동일하게 즉시 놓아준다(공격/포위 유지 방지)
		if (IsDeadOrDyingActor(InstanceData.TargetPlayer) || IsCinematicSuppressedActor(InstanceData.TargetPlayer))
		{
			UEncirclementSubsystem* EncSubsystem = Pawn->GetWorld()->GetSubsystem<UEncirclementSubsystem>();
			AAIController* AICtrl = Context.GetExternalDataPtr(AIControllerHandle);
			UAIPerceptionComponent* PerceptionCompLocal = AICtrl ? AICtrl->GetAIPerceptionComponent() : nullptr;
			ClearTarget(InstanceData, PerceptionCompLocal, EncSubsystem, Pawn);
			InstanceData.TargetLocation = FVector::ZeroVector;
			InstanceData.ChaseLocation = FVector::ZeroVector;
			InstanceData.TimeSinceLastSeen = TargetLostDelay;
		}
		else
		{
			const FVector PawnLoc = Pawn->GetActorLocation();
			InstanceData.TargetLocation = InstanceData.TargetPlayer->GetActorLocation();
			InstanceData.DistanceToTarget = FVector2D::Distance(
				FVector2D(PawnLoc.X, PawnLoc.Y),
				FVector2D(InstanceData.TargetLocation.X, InstanceData.TargetLocation.Y)
				);
			const bool bFreezeChaseLocation = InstanceData.CachedCombatComponent.IsValid()
				&& InstanceData.CachedCombatComponent->IsMovementLockedByAttack();
			
			if (!bFreezeChaseLocation)
			{
				// 플레이어에게 무조건 붙는 이동을 피하게 수정
				const float DirectChaseRange = FMath::Max(InstanceData.AttackableRange + 35.f, 0.f);
				UEncirclementSubsystem* EncSubsystem = Pawn->GetWorld()->GetSubsystem<UEncirclementSubsystem>();
				if (InstanceData.bUseDirectChaseToTarget)
				{
					// DataTable/캐릭터 기본 설정에서 직접 추적을 허용한 몬스터는 플레이어 위치를 ChaseLocation으로 사용.
					// 직접 추적을 끈 몬스터는 전투 거리 안에서 오빗/스트레이프 위치를 사용
					InstanceData.ChaseLocation = InstanceData.TargetLocation;
				}
				else if (InstanceData.DistanceToTarget > DirectChaseRange
					&& (!EncSubsystem
						|| EncSubsystem->CanRequestAttackToken(InstanceData.TargetPlayer, Pawn)))
				{
					// 대안 D(Phase 2 실험): DirectChase는 공격 진입 자격이 있을 때만.
					// 개인 토큰 쿨다운 등으로 CanRequest=false인 몬스터는 아래 슬롯 로직으로 흘려
					// 자연스럽게 Outer로 이동시킨다. Retreat 상태·캐시 도입 없이 목적지 정책만 조건화.
					InstanceData.ChaseLocation = InstanceData.TargetLocation;
				}
				else if (EncSubsystem)
				{
					EncSubsystem->GetOrUpdateRingAnchor(InstanceData.TargetPlayer);
					int32 SlotIndex = EncSubsystem->GetCurrentSlot(InstanceData.TargetPlayer, Pawn);
					if (SlotIndex == INDEX_NONE)
					{
						SlotIndex = EncSubsystem->RequestSlot(InstanceData.TargetPlayer, Pawn);
					}

					// 진단 디버그: 실제 ChaseLocation과 이동 상태를 표시. Encircle.Debug >= 1일 때만.
					// 대안 D 실험 판정(Outer 목적지 생성 vs 실제 이동 성공 여부 구분)에 사용.
					auto DrawChaseLocationDebug = [Pawn, &InstanceData](int32 SlotIdx, bool bHasTok, bool bCanReq, bool bOuter)
					{
						const IConsoleVariable* DebugCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("Encircle.Debug"));
						if (!DebugCVar || DebugCVar->GetInt() <= 0)
						{
							return;
						}
						UWorld* World = Pawn->GetWorld();
						if (!World)
						{
							return;
						}
						const FColor DebugColor = bOuter ? FColor::Cyan : FColor::Red;

						// Outer 목표는 유지되는데 실제 이동이 없는 경우를 구분하기 위한 이동 상태값.
						const float Speed2D = Pawn->GetVelocity().Size2D();
						const AAIController* AIController = Cast<AAIController>(Pawn->GetController());
						FString MoveStatusStr = TEXT("NoAIC");
						if (AIController)
						{
							switch (AIController->GetMoveStatus())
							{
							case EPathFollowingStatus::Idle:    MoveStatusStr = TEXT("Idle");    break;
							case EPathFollowingStatus::Waiting: MoveStatusStr = TEXT("Waiting"); break;
							case EPathFollowingStatus::Paused:  MoveStatusStr = TEXT("Paused");  break;
							case EPathFollowingStatus::Moving:  MoveStatusStr = TEXT("Moving");  break;
							default:                            MoveStatusStr = TEXT("Unknown"); break;
							}
						}

						DrawDebugSphere(World, InstanceData.ChaseLocation, 25.f, 8, DebugColor, false, -1.f);
						DrawDebugLine(World, Pawn->GetActorLocation(), InstanceData.ChaseLocation, DebugColor, false, -1.f);
						DrawDebugString(World, InstanceData.ChaseLocation + FVector(0.f, 0.f, 40.f),
							FString::Printf(TEXT("%s\nSlot=%d HasToken=%d CanRequest=%d Outer=%d\nPawn-Player=%.0f Pawn-Chase=%.0f\nSpeed=%.0f MoveStatus=%s"),
								*Pawn->GetName(), SlotIdx, bHasTok, bCanReq, bOuter,
								FVector::Dist2D(Pawn->GetActorLocation(), InstanceData.TargetLocation),
								FVector::Dist2D(Pawn->GetActorLocation(), InstanceData.ChaseLocation),
								Speed2D, *MoveStatusStr),
							nullptr, DebugColor, 0.f, true);
					};

					if (SlotIndex != INDEX_NONE)
					{
						const bool bHadToken = InstanceData.bHasToken;
						const bool bHasTokenForLocation = EncSubsystem->HasAttackToken(InstanceData.TargetPlayer, Pawn);
						// 토큰을 받을 수 있는(곧 공격할) 적도 안쪽으로 접근해야 사거리에 들어가 공격을 시작할 수 있음
						const bool bCanRequest = EncSubsystem->CanRequestAttackToken(InstanceData.TargetPlayer, Pawn);
						InstanceData.bHasToken = bHasTokenForLocation;
						const bool bUseOuterRadius = !(bHasTokenForLocation || bCanRequest);

						FVector RawTargetLocation = EncSubsystem->GetSlotLocation(InstanceData.TargetPlayer, SlotIndex,
							bUseOuterRadius, InstanceData.StrafeMinNoise, InstanceData.StrafeMaxNoise,
							InstanceData.OrbitInnerRadius, InstanceData.OrbitOuterRadius,
							InstanceData.StrafeOffRange * 0.9f); // 대기자는 Strafe 범위 내에 머무름

						const float JumpSq = FVector::DistSquared(InstanceData.ChaseLocation, RawTargetLocation);
						if (InstanceData.ChaseLocation.IsNearlyZero() || bHadToken != bHasTokenForLocation || JumpSq > FMath::Square(120.f))
						{
							InstanceData.ChaseLocation = RawTargetLocation;
						}
						else
						{
							InstanceData.ChaseLocation = FMath::VInterpTo(
								InstanceData.ChaseLocation, RawTargetLocation, DeltaTime, 7.f);
						}

						// 슬롯 전환 보간이 직선으로 링 중심(플레이어)을 가로지르지 않도록,
						// 보간된 위치가 플레이어에게 너무 가까워지면 각도는 유지한 채 반경만 밀어낸다.
						const FVector ToChase2D = InstanceData.ChaseLocation - InstanceData.TargetLocation;
						const float MinSafeRadiusSq = FMath::Square(InstanceData.OrbitInnerRadius);
						const float DistFromPlayerSq = FVector2D(ToChase2D.X, ToChase2D.Y).SizeSquared();
						if (DistFromPlayerSq < MinSafeRadiusSq && DistFromPlayerSq > KINDA_SMALL_NUMBER)
						{
							const FVector2D SafeDir2D = FVector2D(ToChase2D.X, ToChase2D.Y).GetSafeNormal();
							InstanceData.ChaseLocation.X = InstanceData.TargetLocation.X + SafeDir2D.X * InstanceData.OrbitInnerRadius;
							InstanceData.ChaseLocation.Y = InstanceData.TargetLocation.Y + SafeDir2D.Y * InstanceData.OrbitInnerRadius;
						}

						DrawChaseLocationDebug(SlotIndex, bHasTokenForLocation, bCanRequest, bUseOuterRadius);
					}
					else
					{
						InstanceData.ChaseLocation = EncSubsystem->GetOverflowStandoffLocation(
							InstanceData.TargetPlayer, Pawn, InstanceData.StrafeOffRange);

						// 슬롯을 아예 못 받은(링이 꽉 찬) 대기자 — Outer 취급으로 표시.
						DrawChaseLocationDebug(INDEX_NONE, false, false, true);
					}
				}
				else
				{
					InstanceData.ChaseLocation = InstanceData.TargetLocation;
				}
			}
		}
	}
	
	// 태그 갱신도 매 틱 진행
	if (const IAbilitySystemInterface* ASCIf = Cast<IAbilitySystemInterface>(Pawn))
	{
		if (UAbilitySystemComponent* ASC = ASCIf->GetAbilitySystemComponent())
		{
			InstanceData.OwnedTags.Reset();
			ASC->GetOwnedGameplayTags(InstanceData.OwnedTags);
		}
	}

	if (InstanceData.CachedCombatComponent.IsValid())
	{
		const bool bPatternActive = InstanceData.CachedCombatComponent->IsPatternActive();
		if (bPatternActive)
		{
			InstanceData.bSpecialAttackable = false;
			InstanceData.bAttackable = false;
			InstanceData.bAttackApproachable = false;
		}
		else if (InstanceData.CachedCombatComponent->IsSpecialAttackEvaluationLocked())
		{
			InstanceData.bSpecialAttackable = false;
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
	
	const bool bNewOutOfChaseRange = DistanceFromSpawn >= InstanceData.ChaseRange;

	if (InstanceData.bWasOutOfChaseRange && !bNewOutOfChaseRange)
	{
		// 원점 복귀 완료 → 타깃 즉시 초기화. Suspicious 잔여 게이지도 함께 리셋해서
		// 다음 사이클을 깨끗한 상태로 시작한다.
		UEncirclementSubsystem* EncSubsystem = Pawn->GetWorld()->GetSubsystem<UEncirclementSubsystem>();
		ClearTarget(InstanceData, PerceptionComp, EncSubsystem, Pawn);
		InstanceData.TimeSinceLastSeen = TargetLostDelay;
		InstanceData.SuspicionGauge = 0.f;
		InstanceData.bSuspicionGaugeFull = false;
	}

	InstanceData.bWasOutOfChaseRange = bNewOutOfChaseRange;
	InstanceData.bOutOfChaseRange = bNewOutOfChaseRange;

	TArray<AActor*> PerceivedActors;
	PerceptionComp->GetKnownPerceivedActors(nullptr, PerceivedActors);

	const FVector PawnLocation = Pawn->GetActorLocation();

	// 경계(Suspicious) 단계를 쓰는 몬스터는 최초 발견에 정면 시야각을 요구하지 않는다 —
	// 거리 내에 들어오면 우선 눈치채고, Suspicious 상태에서 서서히 돌아보게 한다.
	const bool bUsesSuspiciousFlow = InstanceData.SuspicionIncreaseRate > 0.f;

	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(Pawn, 0))
	{
		const float PlayerDistSq = FVector::DistSquared(PawnLocation, PlayerPawn->GetActorLocation());
		const AEnemyAIController* EnemyAIController = Cast<AEnemyAIController>(AIController);
		float InitialAcquireRange;
		if (InstanceData.TargetPlayer == nullptr)
		{
			// Idle → 최초 발견: AIPerception Sight 설정(SightRadius)을 그대로 따른다.
			InitialAcquireRange = EnemyAIController
				? EnemyAIController->GetEffectiveSightRadius()
				: InstanceData.ChaseRange;
		}
		else if (bUsesSuspiciousFlow && !InstanceData.bSuspicionGaugeFull)
		{
			// Suspicious 유지: LoseSightRadius 밖으로 나가면 재획득하지 않고 게이지 감소에 위임.
			InitialAcquireRange = EnemyAIController
				? EnemyAIController->GetEffectiveLoseSightRadius()
				: InstanceData.ChaseRange;
		}
		else
		{
			// Combat 유지: ChaseRange까지 끈질기게 붙는다(추격 중 시야 이탈 방지).
			InitialAcquireRange = InstanceData.ChaseRange > 0.f ? InstanceData.ChaseRange : 1500.f;
		}
		if (PlayerDistSq <= FMath::Square(InitialAcquireRange)
			&& AIController->LineOfSightTo(PlayerPawn)
			&& !ContainsActor(PerceivedActors, PlayerPawn))
		{
			PerceivedActors.Add(PlayerPawn);
		}
	}

	AActor* BestTarget = nullptr;
	float BestScore = MAX_FLT;
	float CurrentScore = MAX_FLT;
	const bool bApplyFov = (InstanceData.TargetPlayer == nullptr) && !bUsesSuspiciousFlow;

	UEncirclementSubsystem* EncirclementSubsystem = Pawn->GetWorld()->GetSubsystem<UEncirclementSubsystem>();

	for (AActor* Actor : PerceivedActors)
	{
		if (!Actor || IsDeadOrDyingActor(Actor) || IsCinematicSuppressedActor(Actor))
		{
			continue;
		}
		const AAIController* OwnerController = Cast<AAIController>(Pawn->GetController());
		if (!OwnerController ||
			OwnerController->GetTeamAttitudeTowards(*Actor) != ETeamAttitude::Hostile)
		{
			continue;
		}

		const bool bDamageSensed = WasDamageSensed(PerceptionComp, Actor);
		if (bApplyFov && !bDamageSensed)
		{
			FVector ToTarget = Actor->GetActorLocation() - PawnLocation;
			FVector Forward = Pawn->GetActorForwardVector();
			ToTarget.Z = 0.f;
			Forward.Z = 0.f;
			if (!ToTarget.IsNearlyZero() && !Forward.IsNearlyZero())
			{
				const ARetrieveEnemyCharacter* EnemyForFOV = Cast<ARetrieveEnemyCharacter>(Pawn);
				const float HalfFOVOverride = EnemyForFOV
					? EnemyForFOV->GetHorizontalHalfFOVOverrideForAI()
					: -1.f;
				const float EffectiveHalfFOV = HalfFOVOverride > 0.f
					? FMath::Max(HorizontalHalfFOV, HalfFOVOverride)
					: HorizontalHalfFOV;
				const float CosHalfFov = FMath::Cos(FMath::DegreesToRadians(EffectiveHalfFOV));
				ToTarget.Normalize();
				if (FVector::DotProduct(Forward, ToTarget) < CosHalfFov)
				{
					continue;
				}
			}
		}

		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Pawn->GetWorld());
		if (NavSys)
		{
			FNavLocation ProjectedLoc;
			if (!NavSys->ProjectPointToNavigation(Actor->GetActorLocation(), ProjectedLoc,
			                                      FVector(100.f, 100.f, 250.f)))
			{
				continue;
			}
		}

		const float DistSq = FVector::DistSquared(PawnLocation, Actor->GetActorLocation());
		const int32 Committed = EncirclementSubsystem ? EncirclementSubsystem->GetCommittedCount(Actor) : 0;
		const float Score = DistSq * (1.f + AggroCrowdWeight * Committed);

		if (Actor == InstanceData.TargetPlayer)
		{
			CurrentScore = Score;
		}
		if (Score < BestScore)
		{
			BestScore = Score;
			BestTarget = Actor;
		}
	}

	AActor* ChosenTarget = BestTarget;
	if (IsValid(InstanceData.TargetPlayer) && CurrentScore < MAX_FLT)
	{
		if (BestTarget != InstanceData.TargetPlayer && BestScore > CurrentScore * TargetSwitchHysteresis)
		{
			ChosenTarget = InstanceData.TargetPlayer; // 충분히 개선되지 않음 → 유지
		}
	}

	// AlertedTarget은 ChosenTarget 유무·기존 TargetPlayer 보유 여부와 무관하게 먼저 소비한다.
	// Suspicious 상태(TargetPlayer는 이미 있지만 게이지 미완, 즉 ChosenTarget도 이미 non-null)에서도
	// 동료의 알림이 오면 즉시 확신 단계로 승격시켜야 하는데, ChosenTarget 유무로 게이팅하면
	// 자기 시야로 이미 플레이어를 인식 중인(=ChosenTarget이 항상 non-null인) Suspicious 개체는
	// 이 블록에 영영 도달하지 못해 AlertedTarget이 안 지워진 채 그대로 방치된다.
	if (ARetrieveEnemyCharacter* EnemyChar = Cast<ARetrieveEnemyCharacter>(Pawn))
	{
		if (AActor* Alerted = EnemyChar->AlertedTarget)
		{
			if (!IsDeadOrDyingActor(Alerted) && !IsCinematicSuppressedActor(Alerted))
			{
				if (!IsValid(InstanceData.TargetPlayer))
				{
					InstanceData.TargetPlayer = Alerted;
				}
				InstanceData.bTargetLost = false;
				// 동료가 이미 확신하고 전파한 대상이므로 Suspicious를 건너뛰고 바로 Combat 자격을 준다.
				// bSuspicionGaugeFull도 같이 세팅해야 한다 — 이 값은 이번 틱 하단(게이지 갱신 블록)에서야
				// SuspicionGauge 기준으로 다시 계산되는데, 그 전에 else if(TargetPlayer) 분기가
				// 아직 갱신 전(직전 틱)의 false 값을 보고 "즉시 놓친다" 코드를 실행해버리기 때문이다.
				InstanceData.SuspicionGauge = 1.f;
				InstanceData.bSuspicionGaugeFull = true;
			}
			EnemyChar->AlertedTarget = nullptr;
		}
	}

	if (ChosenTarget)
	{
		if (InstanceData.TargetPlayer != ChosenTarget)
		{
			const bool bHadTarget = InstanceData.TargetPlayer != nullptr;
			if (EncirclementSubsystem && InstanceData.TargetPlayer)
			{
				EncirclementSubsystem->ReleaseSlot(InstanceData.TargetPlayer, Pawn);
				EncirclementSubsystem->ReleaseAttackToken(InstanceData.TargetPlayer, Pawn);
			}
			InstanceData.TargetPlayer = ChosenTarget;

			// Suspicious 플로우를 쓰는 몬스터는 여기서 바로 전파하지 않는다 —
			// 경계 게이지가 다 찼을 때(진짜 Combat 진입 시점)로 미룬다 (아래 참고).
			if (!bHadTarget && !bUsesSuspiciousFlow)
			{
				FEnemyPlayerSpottedPayload Payload;
				Payload.SpottedActor = ChosenTarget;
				Payload.SpottedLocation = ChosenTarget->GetActorLocation();
				Payload.InstigatorLocation = Pawn->GetActorLocation();
				Payload.InstigatorEnemy = Pawn;
				UGameplayMessageSubsystem::Get(Pawn->GetWorld()).BroadcastMessage(
					RetrieveGameplayTags::Channel_Enemy_PlayerSpotted, Payload);
			}
		}
		InstanceData.TimeSinceLastSeen = 0.f;
		InstanceData.bTargetLost = false;
	}
	else if (InstanceData.TargetPlayer)
	{
		const bool bIsSuspiciousPhase = bUsesSuspiciousFlow && !InstanceData.bSuspicionGaugeFull;
		if (!bIsSuspiciousPhase)
		{
			// Combat: TargetLostDelay 유예 후 최종 해제
			InstanceData.TimeSinceLastSeen += TickInterval;
			if (InstanceData.TimeSinceLastSeen >= TargetLostDelay)
			{
				ClearTarget(InstanceData, PerceptionComp, EncirclementSubsystem, Pawn);
			}
		}
		// Suspicious: 최종 소실은 아래 게이지 감소 블록이 거리 조건으로 직접 판정하고 처리한다.
	}

	if (bUsesSuspiciousFlow)
	{
		// 게이지 판정은 known 목록/ChosenTarget과 무관하게 실제 거리로 확정한다.
		// AISense_Sight의 known은 MaxAge(5s) 만큼 잔존하므로 그것만으로는 시야 이탈을 판정할 수 없다.
		// bTargetLost는 아래 게이지 0 도달 시점에서만 세팅해 StateTree 전이 세맨틱을 보존한다.
		const AEnemyAIController* EnemyAICtrlForGauge = Cast<AEnemyAIController>(AIController);
		const float LoseRangeForGauge = EnemyAICtrlForGauge
			? EnemyAICtrlForGauge->GetEffectiveLoseSightRadius()
			: InstanceData.ChaseRange;
		const float DistToTargetForGauge = IsValid(InstanceData.TargetPlayer)
			? FVector2D::Distance(
				FVector2D(Pawn->GetActorLocation()),
				FVector2D(InstanceData.TargetPlayer->GetActorLocation()))
			: MAX_FLT;
		// LOS도 함께 검사 — AIPerception known은 MaxAge(5s) 동안 잔존하므로 거리만으로는
		// 벽 뒤로 이동한 플레이어가 계속 감지된 것으로 오판돼 게이지가 증가할 수 있다.
		const bool bTargetInSight = IsValid(InstanceData.TargetPlayer)
			&& DistToTargetForGauge <= LoseRangeForGauge
			&& AIController->LineOfSightTo(InstanceData.TargetPlayer);
		if (bTargetInSight)
		{
			// DistanceToTarget은 타겟을 처음 인식한 이번 틱엔 아직 갱신 전(이전 틱 값=0)이라
			// 여기서 직접 신선한 거리를 계산한다 — 안 그러면 최초 인식 시 항상 0<=ForceCombatRange로 오판된다.
			const float FreshDistanceToTarget = FVector2D::Distance(
				FVector2D(Pawn->GetActorLocation()), FVector2D(InstanceData.TargetPlayer->GetActorLocation()));
			const bool bForceCombat = InstanceData.ForceCombatRange > 0.f
				&& FreshDistanceToTarget <= InstanceData.ForceCombatRange;
			if (WasDamageSensed(PerceptionComp, InstanceData.TargetPlayer) || bForceCombat)
			{
				// 피격 감지 또는 강제 전투 진입 거리 이내 — 이미 확실히 들켰으므로 Suspicious를 건너뛴다.
				InstanceData.SuspicionGauge = 1.f;
			}
			else
			{
				InstanceData.SuspicionGauge = FMath::Clamp(
					InstanceData.SuspicionGauge + InstanceData.SuspicionIncreaseRate * TickInterval, 0.f, 1.f);
			}
		}
		else if (InstanceData.SuspicionGauge > 0.f)
		{
			InstanceData.SuspicionGauge = FMath::Clamp(
				InstanceData.SuspicionGauge - InstanceData.SuspicionDecreaseRate * TickInterval, 0.f, 1.f);

			if (InstanceData.SuspicionGauge <= 0.f && IsValid(InstanceData.TargetPlayer))
			{
				// 게이지 소진 → 최종 소실 처리. ClearTarget이 bTargetLost=true 세팅 → Suspicious→Idle 전이 자연 발동.
				ClearTarget(InstanceData, PerceptionComp, EncirclementSubsystem, Pawn);
			}
		}

		InstanceData.bSuspicionGaugeFull = InstanceData.SuspicionGauge >= 1.f;

		if (InstanceData.bSuspicionGaugeFull && !InstanceData.bWasSuspicionGaugeFull && IsValid(InstanceData.TargetPlayer))
		{
			FEnemyPlayerSpottedPayload Payload;
			Payload.SpottedActor = InstanceData.TargetPlayer;
			Payload.SpottedLocation = InstanceData.TargetPlayer->GetActorLocation();
			Payload.InstigatorLocation = Pawn->GetActorLocation();
			Payload.InstigatorEnemy = Pawn;
			UGameplayMessageSubsystem::Get(Pawn->GetWorld()).BroadcastMessage(
				RetrieveGameplayTags::Channel_Enemy_PlayerSpotted, Payload);
		}
		InstanceData.bWasSuspicionGaugeFull = InstanceData.bSuspicionGaugeFull;

		if (InstanceData.CachedSuspicionIndicator.IsValid())
		{
			InstanceData.CachedSuspicionIndicator->SetSuspicionGauge(InstanceData.SuspicionGauge);
		}
	}
	
	if (InstanceData.CachedCombatComponent.IsValid())
	{
		if (UEncirclementSubsystem* EncircleSubsystem = Pawn->GetWorld()->GetSubsystem<UEncirclementSubsystem>())
		{
			const bool bHasValidTarget = IsValid(InstanceData.TargetPlayer);
			
			InstanceData.bHasToken = bHasValidTarget
				&& EncircleSubsystem->HasAttackToken(InstanceData.TargetPlayer, Pawn);

			const bool bCanRequestToken = bHasValidTarget
				&& EncircleSubsystem->CanRequestAttackToken(InstanceData.TargetPlayer, Pawn);

			const bool bPatternActive = InstanceData.CachedCombatComponent->IsPatternActive();
			const bool bSpecialAttackEvaluationLocked =
				InstanceData.CachedCombatComponent->IsSpecialAttackEvaluationLocked();
			const bool bSpecialAttackRetryCooldownReady =
				InstanceData.CachedCombatComponent->IsSpecialAttackRetryCooldownReady();

			const ARetrieveEnemyCharacter* EnemyCharacter = Cast<ARetrieveEnemyCharacter>(Pawn);
			const UCharacterMovementComponent* CharacterMovement = Pawn->FindComponentByClass<UCharacterMovementComponent>();
			const bool bShouldSuppressNormalAttackWhileFlying = EnemyCharacter
				&& EnemyCharacter->ShouldSuppressNormalAttackWhileFlying()
				&& CharacterMovement
				&& CharacterMovement->MovementMode == MOVE_Flying;
			const bool bUsePatternRangeForNormalAttack = EnemyCharacter
				&& EnemyCharacter->ShouldUsePatternRangeForNormalAttack();
			const bool bAttackPatternAvailable =
				InstanceData.CachedCombatComponent->IsAttackable(InstanceData.TargetPlayer);
			const bool bNormalAttackable = bCanRequestToken
				&& !bPatternActive
				&& !bShouldSuppressNormalAttackWhileFlying
				&& bAttackPatternAvailable
				&& (bUsePatternRangeForNormalAttack || InstanceData.DistanceToTarget <= InstanceData.AttackableRange);

			// Attack 상태 진입 전용 판정. bAttackPatternAvailable(=MaxActivationRange 기준)만 확인해서
			// AttackableRange 안으로 접근하는 동안(Attack Task 내부 이동)에도 Attack 상태를 유지할 수 있게 한다.
			const bool bNormalAttackApproachable = bCanRequestToken
				&& !bPatternActive
				&& !bShouldSuppressNormalAttackWhileFlying
				&& bAttackPatternAvailable;

			// 특수 공격 가능 여부 — 일반/보스는 원본 동작 그대로(일반 공격 가능 여부와 독립적으로
			// 쿨다운/락만으로 판정). 에픽도 동일하게 쿨다운 완료 시 발동 가능.
			const bool bRawSpecialAttackable = bHasValidTarget
				&& !bPatternActive
				&& !bSpecialAttackEvaluationLocked
				&& bSpecialAttackRetryCooldownReady
				&& InstanceData.CachedCombatComponent->HasAvailablePatternByType(
					InstanceData.TargetPlayer, RetrieveGameplayTags::Ability_Enemy_SpecialAttack);

			InstanceData.bSpecialAttackable = bRawSpecialAttackable;

			if (EnemyCharacter && EnemyCharacter->ShouldUsePatternRangeForNormalAttack())
			{
				InstanceData.bHasAerialPhase = EnemyCharacter
					&& EnemyCharacter->ShouldUseStateTreeAerialPhase()
					&& !EnemyCharacter->IsAerialSpecialAttackReady()
					&& bRawSpecialAttackable;
				const bool bEpicHasToken = bHasValidTarget
					&& EncircleSubsystem->HasAttackToken(InstanceData.TargetPlayer, Pawn);
				InstanceData.bHasToken = bEpicHasToken;
				InstanceData.bAttackable = bNormalAttackable;
				InstanceData.bAttackApproachable = bNormalAttackApproachable;
			}
			else
			{
				// 일반/보스: 원본 동작 그대로. 일반 공격과 특수 공격 판정은 서로 독립적이며
				// StateTree 전이 우선순위가 선택을 담당한다.
				InstanceData.bHasAerialPhase = false;
				InstanceData.bAttackable = bNormalAttackable;
				InstanceData.bAttackApproachable = bNormalAttackApproachable;
			}
		}
		else
		{
			InstanceData.bHasToken   = false;
			InstanceData.bAttackable = false;
			InstanceData.bAttackApproachable = false;
			InstanceData.bSpecialAttackable = false;
			InstanceData.bHasAerialPhase = false;
		}
	}
	else
	{
		InstanceData.bHasToken   = false;
		InstanceData.bAttackable = false;
		InstanceData.bAttackApproachable = false;
		InstanceData.bSpecialAttackable = false;
		InstanceData.bHasAerialPhase = false;
	}

	if (AEnemyAIController* EnemyAIController = Cast<AEnemyAIController>(AIController))
	{
		EnemyAIController->SetRecognizedTarget(InstanceData.bOutOfChaseRange ? nullptr : InstanceData.TargetPlayer);
	}
}
