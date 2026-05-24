

#include "LockOnComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Camera/PlayerCameraManager.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Kismet/GameplayStatics.h"    
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"

ULockOnComponent::ULockOnComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// 기본 ObjectType: Pawn
	SearchObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
}

void ULockOnComponent::BeginPlay()
{
	Super::BeginPlay();
}

void ULockOnComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopLockOn();
	UWorld* World = GetWorld();
	if (IsValid(World))
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	
	Super::EndPlay(EndPlayReason);
}

bool ULockOnComponent::StartLockOn()
{
	if (CachePlayerRefs() == false)
	{
		return false;
	}
	
	TArray<AActor*> Candidates;
	FindCandidates(Candidates);
	
	if (Candidates.IsEmpty())
	{
		return false;
	}
	
	AActor* Best = PickBestTarget(Candidates);
	if (IsValid(Best) == false)
	{
		return false;
	}
	
	SetCurrentTarget(Best);
	
	return true;
}

void ULockOnComponent::StopLockOn()
{
	SetCurrentTarget(nullptr);
}

bool ULockOnComponent::SwitchTarget(FVector2D InputDir)
{
	// 입력 임계값 미만이면 무시
	if (FMath::Abs(InputDir.X) < SwitchInputThreshold)
	{
		return false;
	}
	
	// 락온 중이 아니면 의미 X
	if (IsLockedOn() == false)
	{
		return false;
	}
	
	if (CachePlayerRefs() == false)
	{
		return false;
	}
	
	TArray<AActor*> Candidates;
	FindCandidates(Candidates);
	
	if (Candidates.IsEmpty())
	{
		return false;
	}
	
	AActor* Switched = PickSwitchTarget(Candidates, InputDir);
	if (IsValid(Switched) == false)
	{
		return false;
	}
	
	SetCurrentTarget(Switched);
	return true;
}

bool ULockOnComponent::Toggle()
{
	UE_LOG(LogTemp, Warning, TEXT("[LockOnComp] Toggle called, bLockOnActive=%d"), bLockOnActive);
	if (IsLockedOn())
	{
		StopLockOn();
		return false;
	}
	return StartLockOn();
}

void ULockOnComponent::MonitorTick()
{
	// MonitorInterval 주기로 락온 체크 -> 추후 사망 조건은 ASC RegisterGameplayTagEvent 구독으로 전환
	if (ShouldBreakLockOn())
	{
		StopLockOn();
		return;
	}
	// 락온 유지 중 - 디버그 시각화
	DrawDebugLockOn();
}

void ULockOnComponent::DrawDebugLockOn() const
{
	if (bDebugDraw == false)
	{
		return;
	}
	AActor* Target = CurrentTarget.Get();
	if (IsValid(Target) == false)
	{
		return;
	}
	
	UWorld* World = GetWorld();
	if (IsValid(World) == false)
	{
		return;
	}
	
	const FVector TargetLoc = Target->GetActorLocation();
	
	// 타겟 위에 노란 구체 (락온 표시)
	DrawDebugSphere(World, TargetLoc, 80.f, 12, FColor::Yellow, false, MonitorInterval, 0, 2.f);
	// 카메라 -> 타겟 노란 라인
	APlayerCameraManager* CamManager = CachedCameraManager.Get();
	if (IsValid(CamManager) == false)
	{
		return;
	}

	const FVector CamLoc = CamManager->GetCameraLocation();
	DrawDebugLine(World, CamLoc, TargetLoc, FColor::Yellow, false, MonitorInterval, 0, 2.f);
}

APawn* ULockOnComponent::GetOwningPawn() const
{
	return Cast<APawn>(GetOwner());
}

bool ULockOnComponent::CachePlayerRefs()
{
	APawn* OwnerPawn = GetOwningPawn();
	if (IsValid(OwnerPawn) == false)
	{
		return false;
	}
	
	APlayerController* CurrentPC = Cast<APlayerController>(OwnerPawn->GetController());
	if (IsValid(CurrentPC) == false)
	{
		return false;
	}
	
	// 캐시가 현재 컨트롤러와 일치하면 그대로
	if (CachedPC.Get() == CurrentPC && CachedCameraManager.IsValid())
	{
		return true;
	}
	// 컨트롤러 교체됐다면 캐시 갱신
	CachedPC = CurrentPC;
	CachedCameraManager = CurrentPC->PlayerCameraManager;
	
	return CachedCameraManager.IsValid();
}

void ULockOnComponent::FindCandidates(TArray<AActor*>& OutCandidates) const
{
	OutCandidates.Reset();
	// CachePlayerRefs는 호출자가 미리 보장 전제
	// 여기서는 캐시가 비어있으면 빈 결과 리턴
	APlayerCameraManager* CamManager = CachedCameraManager.Get();
	if (IsValid(CamManager) == false)
	{
		return;
	}
	APawn* OwnerPawn = GetOwningPawn();
	
	const FVector CamLoc = CamManager->GetCameraLocation();
	const FRotator CamRot = CamManager->GetCameraRotation();
	const FVector TraceEnd = CamLoc + CamRot.Vector() * MaxDistance;
	// 자기 자신 무시
	TArray<AActor*> IgnoreActors;
	if (IsValid(OwnerPawn))
	{
		IgnoreActors.Add(OwnerPawn);
	}
	
	// SphereTrace
	TArray<FHitResult> Hits;
	UKismetSystemLibrary::SphereTraceMultiForObjects(
		this,
		CamLoc,
		TraceEnd,
		SphereRadius,
		SearchObjectTypes,
		false,
		IgnoreActors,
		bDebugDraw ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None,
		Hits,
		true
		);	
	
	// 중복 제거 + 시야 검증
	TSet<AActor*> Seen;
	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();
		if (IsValid(HitActor) == false)
		{
			continue;
		}
		if (Seen.Contains(HitActor))
		{
			continue;
		}
		Seen.Add(HitActor);
		
		if (HasLineOfSight(HitActor) == false)
		{
			continue;
		}
		
		OutCandidates.Add(HitActor);
	}
}

bool ULockOnComponent::HasLineOfSight(const AActor* Target) const
{
	if (IsValid(Target) == false)
	{
		return false;
	}
	
	APlayerCameraManager* CamManager = CachedCameraManager.Get();
	if (IsValid(CamManager) == false)
	{
		return false;
	}
	
	UWorld* World = GetWorld();
	if (IsValid(World) == false)
	{
		return false;
	}
	
	const FVector Start = CamManager->GetCameraLocation();
	const FVector End = Target->GetActorLocation();
	
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(GetOwningPawn());
	Params.AddIgnoredActor(Target);
	
	FHitResult Hit;
	const bool bBlocked = World->LineTraceSingleByChannel(
		Hit, Start, End, LineOfSightChannel, Params
		);
	
	if (bDebugDraw)
	{
		const FColor LineColor = bBlocked ? FColor::Red : FColor::Green;
		DrawDebugLine(World, Start, End, LineColor, false, 0.1f, 0, 1.f);
	}
	
	return bBlocked == false;
}

bool ULockOnComponent::ProjectToScreen(const AActor* Actor, FVector2D& OutScreen, bool& bOutInFront) const
{
	OutScreen = FVector2D::ZeroVector;
	bOutInFront = false;
	
	if (IsValid(Actor) == false)
	{
		return false;
	}
	
	APlayerController* PC = CachedPC.Get();
	if (IsValid(PC) == false)
	{
		return false;
	}
	
	const FVector WorldLoc = Actor->GetActorLocation();
	// 카메라 앞/뒤 판정
	APlayerCameraManager* CamManager = CachedCameraManager.Get();
	if (IsValid(CamManager))
	{
		const FVector CamLoc = CamManager->GetCameraLocation();
		const FVector CamFwd = CamManager->GetCameraRotation().Vector();
		const FVector ToActor = WorldLoc - CamLoc;
		bOutInFront = FVector::DotProduct(CamFwd, ToActor) > 0.f;
	}
	// 스크린 좌표 계산
	const bool bProjected = UGameplayStatics::ProjectWorldToScreen(
		PC,
		WorldLoc,
		OutScreen,
		false
		);
	
	return bProjected;
}

AActor* ULockOnComponent::PickBestTarget(const TArray<AActor*>& Candidates) const
{
	if (Candidates.IsEmpty())
	{
		return nullptr;
	}
	
	APlayerController* PC = CachedPC.Get();
	if (IsValid(PC) == false)
	{
		return nullptr;
	}
	
	int32 ViewportSizeX = 0;
	int32 ViewportSizeY = 0;
	PC->GetViewportSize(ViewportSizeX, ViewportSizeY);
	if (ViewportSizeX <= 0 || ViewportSizeY <= 0)
	{
		return nullptr;
	}
	
	const FVector2D ScreenCenter(ViewportSizeX * 0.5f, ViewportSizeY * 0.5f);
	// 화면 절반 대각선 길이 
	const float ScreenHalfDiagonal = FMath::Sqrt(
		FMath::Square(ViewportSizeX * 0.5f) + 
		FMath::Square(ViewportSizeY * 0.5f)
		);
	// 카메라 위치
	APlayerCameraManager* CamManager = CachedCameraManager.Get();
	if (IsValid(CamManager) == false)
	{
		return nullptr;
	}
	const FVector CamLoc = CamManager->GetCameraLocation();
	
	AActor* BestActor = nullptr;
	float BestScore = -1.f;
	for (AActor* Candidate : Candidates)
	{
		if (IsValid(Candidate) == false)
		{
			continue;
		}
		// 스크린 좌표 변환
		FVector2D ScreenPos;
		bool bInFront = false;
		if (ProjectToScreen(Candidate, ScreenPos, bInFront) == false)
		{
			continue;
		}
		
		if (bInFront == false)
		{
			continue;
		}
		
		// 화면 중앙으로부터 정규화 거리(0 ~ 1)
		const float ScreenDist = (ScreenPos - ScreenCenter).Size();
		const float NormalizedScreenDist = FMath::Clamp(ScreenDist / ScreenHalfDiagonal, 0.f, 1.f);
		// 데드존 컷
		if (NormalizedScreenDist > MaxScreenDistance)
		{
			continue;
		}
		
		// 월드 거리 정규화
		const float WorldDist = FVector::Distance(Candidate->GetActorLocation(), CamLoc);
		const float NormalizedWorldDist = FMath::Clamp(WorldDist / MaxDistance, 0.f, 1.f);
		// 스코어 계산 - 가까울수록 고점
		const float Score = ScreenWeight * (1.f - NormalizedScreenDist) + DistanceWeight * (1.f - NormalizedWorldDist);
		
		if (Score > BestScore)
		{
			BestScore = Score;
			BestActor = Candidate;
		}
	}
	
	return BestActor;
}

AActor* ULockOnComponent::PickSwitchTarget(const TArray<AActor*>& Candidates, FVector2D InputDir) const
{
	AActor* Current = CurrentTarget.Get();
	if (IsValid(Current) == false)
	{
		return nullptr;
	}
	// 현재 타겟의 스크린 좌표
	FVector2D CurrentScreenPos;
	bool bCurrentInFront = false;
	if (ProjectToScreen(Current, CurrentScreenPos, bCurrentInFront) == false)
	{
		return nullptr;
	}
	
	if (bCurrentInFront == false)
	{
		return nullptr;
	}
	// 방향 부호 양수 == 우측 음수 == 좌측
	const float DirSign = InputDir.X > 0.f ? 1.f : -1.f;
	AActor* BestActor = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	
	for (AActor* Candidate : Candidates)
	{
		if (IsValid(Candidate) == false || Candidate == Current)
		{
			continue;
		}
		FVector2D ScreenPos;
		bool bInFront = false;
		if (ProjectToScreen(Candidate, ScreenPos, bInFront) == false)
		{
			continue;
		}
		if (bInFront == false)
		{
			continue;
		}
		
		// 현재 타겟 대비 가로 오프셋이 요청한 방향과 다르면 제외
		const float DeltaX = ScreenPos.X - CurrentScreenPos.X;
		if (DirSign * DeltaX <= 0.f)
		{
			continue;
		}
		// 가로로 가장 가까운 후보 선택
		const float HorizontalDist = FMath::Abs(DeltaX);
		if (HorizontalDist < BestDist)
		{
			BestDist = HorizontalDist;
			BestActor = Candidate;
		}
	}
	return BestActor;
}

void ULockOnComponent::SetCurrentTarget(AActor* NewTarget)
{
	// 파괴중인 액터를 타겟으로 잡는 상황 대비
	if (IsValid(NewTarget) == false)
	{
		NewTarget = nullptr;
	}
	
	AActor* Old = CurrentTarget.Get();
	const bool bWasActive = bLockOnActive;
	const bool bNewActive = IsValid(NewTarget);
	// 같은 타겟 + 시스템 상태(bLockOnActive) 동일 -> 변경 없음
	if (Old == NewTarget && bWasActive == bNewActive)
	{
		return;
	}
	
	CurrentTarget = NewTarget;
	bLockOnActive = bNewActive;
	
	const bool bLockStarted = bNewActive && !bWasActive;
	const bool bLockEnded = !bNewActive && bWasActive;
	
	UWorld* World = GetWorld();
	// 락온 진입/이탈에 따른 태그 + 모니터 타이머 토글
	if (bLockStarted)
	{
		ApplyLockOnTag(true);
		if (IsValid(World))
		{
			World->GetTimerManager().SetTimer(
				MonitorTimerHandle,
				this,
				&ULockOnComponent::MonitorTick,
				MonitorInterval,
				true
				);
		}
	}
	else if (bLockEnded)
	{
		ApplyLockOnTag(false);
		if (IsValid(World))
		{
			World->GetTimerManager().ClearTimer(MonitorTimerHandle);
		}
	}
	
	OnTargetChanged.Broadcast(NewTarget);
}

bool ULockOnComponent::ShouldBreakLockOn() const
{
	if (bLockOnActive == false)
	{
		return false;
	}
	
	// 타겟 무효
	AActor* Target = CurrentTarget.Get();
	if (IsValid(Target) == false)
	{
		return true;
	}
	// 거리 초과
	APawn* OwnerPawn = GetOwningPawn();
	if (IsValid(OwnerPawn) == false)
	{
		return true;
	}
	
	const float DistSquared = FVector::DistSquared(Target->GetActorLocation(), OwnerPawn->GetActorLocation());
	if (DistSquared > FMath::Square(BreakDistance))
	{
		return true;
	}
	// 시야 차단
	if (HasLineOfSight(Target) == false)
	{
		return true;
	}
	// 사망 
	if (IsTargetDead(Target))
	{
		return true;
	}
	
	return false;
}

bool ULockOnComponent::IsTargetDead(const AActor* Target) const
{
	if (IsValid(Target) == false)
	{
		return false;
	}
	
	const IAbilitySystemInterface* ASCInterface = Cast<const IAbilitySystemInterface>(Target);
	if (ASCInterface == nullptr)
	{
		// ASC 없는 액터는 사망 판정 불가
		return false;
	}
	
	UAbilitySystemComponent* ASC = ASCInterface->GetAbilitySystemComponent();
	if (IsValid(ASC) == false)
	{
		return false;
	}
	
	// Enemy / Boss 사망 둘다 체크
	return ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Enemy_Dead)
	|| ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Boss_Dead);
}

void ULockOnComponent::ApplyLockOnTag(bool bAdd) const
{
	APawn* OwnerPawn = GetOwningPawn();
	if (IsValid(OwnerPawn) == false)
	{
		return;
	}
	
	IAbilitySystemInterface* ASCInterface = Cast<IAbilitySystemInterface>(OwnerPawn);
	if (ASCInterface == nullptr)
	{
		return;
	}
	
	UAbilitySystemComponent* ASC = ASCInterface->GetAbilitySystemComponent();
	if (IsValid(ASC) == false)
	{
		return;
	}
	
	if (bAdd)
	{
		ASC->AddLooseGameplayTag(RetrieveGameplayTags::LockOn_Active);
	}
	else
	{
		ASC->RemoveLooseGameplayTag(RetrieveGameplayTags::LockOn_Active);
	}
}
