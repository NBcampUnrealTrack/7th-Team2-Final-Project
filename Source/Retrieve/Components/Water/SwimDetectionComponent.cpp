
#include "SwimDetectionComponent.h"

#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/CapsuleComponent.h"
#include "Components/Combat/CombatStanceComponent.h"
#include "Components/Pawn/RetrieveCharacterMovementComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Interface/RetrieveWaterProvider.h"
#include "Settings/RetrieveSwimSettings.h"
#include "TimerManager.h"

USwimDetectionComponent::USwimDetectionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // 물 영역 안에서만 동작
}

void USwimDetectionComponent::TickComponent(float DeltaTime, ELevelTick LevelTick,
	FActorComponentTickFunction* ActorComponentTickFunction)
{
	Super::TickComponent(DeltaTime, LevelTick, ActorComponentTickFunction);
	if (!OwnerCharacter)
	{
		return;
	}

	UCharacterMovementComponent* CMC = OwnerCharacter->GetCharacterMovement();
	if (!CMC) { return; }

	// 수면 깊이 = 수면Z - 액터Z(캡슐 중심). Provider가 평면 상수 / 강 스플라인 계산.
	const FVector Loc = OwnerCharacter->GetActorLocation();
	float SurfaceZ = 0.f;
	const bool bInColumn = ResolveCurrentWater(Loc, SurfaceZ);
	URetrieveCharacterMovementComponent* RetrieveCMC = Cast<URetrieveCharacterMovementComponent>(CMC);
	if (!bInColumn)
	{
		if (bSwimming)
		{
			SetSwimming(false);
		}
		if (RetrieveCMC)
		{
			RetrieveCMC->ClearWaterState();
		}
		return;
	}
	const float DepthDiff = bInColumn ? (SurfaceZ - Loc.Z) : -UE_BIG_NUMBER;

	// 캡슐 기준 잠수 판정. 완전잠수 = 캡슐 상단 < 수면. 잠수비율 = 발끝(0) ~ 머리(1).
	const UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
	const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.f;
	const bool bFullySubmerged = bInColumn && (Loc.Z + HalfHeight < SurfaceZ);
	const float Submersion = bInColumn
		? FMath::Clamp((DepthDiff + HalfHeight) / (2.f * HalfHeight), 0.f, 1.f)
		: 0.f;

	const bool bSuppressed = (WaterSuppressCount > 0);
	const bool bHasFloor = HasFloorBelow();

	// 수영 = 물기둥 안 && 비억제 && 충분히 잠김. 표면/브리치 동작은 전부 이 식이 결정 → 건드리지 않음.
	const float WalkMax = GetDefault<URetrieveSwimSettings>()->WalkMaxSubmersion;
	const float SwimThreshold = bSwimming ? (WalkMax - 0.08f) : (WalkMax + 0.08f);
	bool bWantSwim = bInColumn && !bSuppressed && (Submersion > SwimThreshold);

	// 외과적: 발밑 바닥 있고 + 수면이 "부력 평형선(FloatOffset)" 아래면 = 설 수 있는 얕은 곳 → 걷기.
	// FloatOffset = 캡슐이 떠 있는 높이. 물이 이 선에 닿으면(DepthDiff > FloatOffset) 수영 시작.
	// (정수리 기준이면 완전 잠길 때까지 걷다가 IK로 수장됨 → 평형선 기준으로 더 일찍 수영 전환)
	const float FloatLine = GetDefault<URetrieveSwimSettings>()->FloatOffset;
	if (bWantSwim && bHasFloor && (Loc.Z + FloatLine) > SurfaceZ)
	{
		bWantSwim = false;
	}
	if (!bSwimming && bWantSwim)
	{
		SetSwimming(true);
	}
	else if (bSwimming && !bWantSwim)
	{
		SetSwimming(false);
		// 이탈 후에도 물기둥 안이면(Wade/Submerged-Walk) 항력·점프게이트 유지.
		if (RetrieveCMC) { RetrieveCMC->SetWaterState(SurfaceZ, Submersion, bInColumn, bFullySubmerged); }
		return;
	}

	// 물 상태를 CMC에 매 틱 push (걷기/수영 공통: 부력·항력·완전잠수 점프게이트).
	if (RetrieveCMC) { RetrieveCMC->SetWaterState(SurfaceZ, Submersion, bInColumn, bFullySubmerged); }

	if (bSwimming)
	{
		// 완전잠수 = UnderWater 태그(PP/애님/수직입력 분기 + Sprint 해제).
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerCharacter))
		{
			ASC->SetLooseGameplayTagCount(RetrieveGameplayTags::State_Player_Swimming_UnderWater, bFullySubmerged ? 1 : 0);
			if (bFullySubmerged)
			{
				ASC->SetLooseGameplayTagCount(RetrieveGameplayTags::State_Player_Sprinting, 0);
			}
		}

		const bool bPlunging = RetrieveCMC ? RetrieveCMC->IsPlunging() : false;

		// 상하 트림. 플런지 중엔 입력 무시(순수 모멘텀).
		if (VerticalInput != 0.f && !bPlunging)
		{
			OwnerCharacter->AddMovementInput(FVector::UpVector, VerticalInput);
		}
	}
}

void USwimDetectionComponent::ChangeWaterSuppress(int32 Delta)
{
	WaterSuppressCount = FMath::Max(0, WaterSuppressCount + Delta);
}

void USwimDetectionComponent::NotifyEnterWaterRegion(const TScriptInterface<IRetrieveWaterProvider>& InWater)
{
	if (!InWater.GetObject())
	{
		return;
	}

	for (const TScriptInterface<IRetrieveWaterProvider>& Candidate : CandidateWaters)
	{
		if (Candidate.GetObject() == InWater.GetObject())
		{
			SetComponentTickEnabled(true);
			return;
		}
	}

	CandidateWaters.Add(InWater);
	SetComponentTickEnabled(true);
}

void USwimDetectionComponent::NotifyExitWaterRegion(const TScriptInterface<IRetrieveWaterProvider>& InWater)
{
	UObject* WaterObject = InWater.GetObject();
	if (!WaterObject)
	{
		return;
	}

	for (int32 i = CandidateWaters.Num() - 1; i >= 0; --i)
	{
		if (CandidateWaters[i].GetObject() == WaterObject)
		{
			CandidateWaters.RemoveAtSwap(i);
		}
	}

	if (CandidateWaters.Num() > 0)
	{
		if (CurrentWater.GetObject() == WaterObject && OwnerCharacter)
		{
			float SurfaceZ = 0.f;
			if (!ResolveCurrentWater(OwnerCharacter->GetActorLocation(), SurfaceZ))
			{
				if (bSwimming)
				{
					SetSwimming(false);
				}
				if (URetrieveCharacterMovementComponent* RetrieveCMC = Cast<URetrieveCharacterMovementComponent>(OwnerCharacter->GetCharacterMovement()))
				{
					RetrieveCMC->ClearWaterState();
				}
			}
		}
		return;
	}

	if (bSwimming)
	{
		SetSwimming(false);
	}
	// 물 영역 완전 이탈 → 항력/점프게이트 리셋.
	if (OwnerCharacter)
	{
		if (URetrieveCharacterMovementComponent* RetrieveCMC = Cast<URetrieveCharacterMovementComponent>(OwnerCharacter->GetCharacterMovement()))
		{
			RetrieveCMC->ClearWaterState();
		}
	}
	const bool bHadActiveWater = CurrentWater.GetObject() != nullptr;
	CurrentWater = TScriptInterface<IRetrieveWaterProvider>();
	if (bHadActiveWater)
	{
		OnSwimWaterRegionChanged.Broadcast(false);
	}
	SetComponentTickEnabled(false);
}

void USwimDetectionComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerCharacter = Cast<ACharacter>(GetOwner());

	// 스폰/로드 시 이미 물 안이면 BeginOverlap이 안 뜸 → 다음 틱 초기 검사
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &USwimDetectionComponent::CheckInitialWaterOverlap);
	}
}

void USwimDetectionComponent::CheckInitialWaterOverlap()
{
	if (!OwnerCharacter || CandidateWaters.Num() > 0) // 이미 진입했으면 skip(멱등)
	{
		return;
	}

	const UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
	if (!Capsule) { return; }

	TArray<AActor*> Overlapping;
	Capsule->GetOverlappingActors(Overlapping);
	for (const AActor* Actor : Overlapping)
	{
		for (UActorComponent* Comp : Actor->GetComponents())
		{
			if (Comp && Comp->Implements<URetrieveWaterProvider>())
			{
				NotifyEnterWaterRegion(TScriptInterface<IRetrieveWaterProvider>(Comp));
			}
		}
	}
}

void USwimDetectionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (OwnerCharacter) // 수영 중 파괴 시 수중 태그 잔류 방지
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerCharacter))
		{
			ASC->SetLooseGameplayTagCount(RetrieveGameplayTags::State_Player_Swimming_UnderWater, 0);
		}
	}
	Super::EndPlay(EndPlayReason);
}

bool USwimDetectionComponent::HasFloorBelow() const
{
	const UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
	if (!Capsule) { return false; }

	const FVector Start = OwnerCharacter->GetActorLocation();
	const float  HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector End = Start - FVector(0.f, 0.f, HalfHeight + GetDefault<URetrieveSwimSettings>()->WadeFloorDistance);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SwimFloorCheck), false, OwnerCharacter);
	// 채널은 바닥이 막는 것으로(ECC_WorldStatic 등). Visibility가 바닥 포함 안 하면 교체.
	return GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params);
}

bool USwimDetectionComponent::ResolveCurrentWater(const FVector& Location, float& OutSurfaceZ)
{
	UObject* PreviousWater = CurrentWater.GetObject();

	for (int32 i = CandidateWaters.Num() - 1; i >= 0; --i)
	{
		if (!CandidateWaters[i].GetObject())
		{
			CandidateWaters.RemoveAtSwap(i);
		}
	}

	if (PreviousWater)
	{
		for (const TScriptInterface<IRetrieveWaterProvider>& Candidate : CandidateWaters)
		{
			if (Candidate.GetObject() == PreviousWater &&
				IRetrieveWaterProvider::Execute_TryGetWaterColumn(PreviousWater, Location, OutSurfaceZ))
			{
				return true;
			}
		}
	}

	for (const TScriptInterface<IRetrieveWaterProvider>& Candidate : CandidateWaters)
	{
		UObject* CandidateObject = Candidate.GetObject();
		if (CandidateObject && IRetrieveWaterProvider::Execute_TryGetWaterColumn(CandidateObject, Location, OutSurfaceZ))
		{
			CurrentWater = Candidate;
			if (PreviousWater != CandidateObject)
			{
				OnSwimWaterRegionChanged.Broadcast(true);
			}
			return true;
		}
	}

	CurrentWater = TScriptInterface<IRetrieveWaterProvider>();
	if (PreviousWater)
	{
		OnSwimWaterRegionChanged.Broadcast(false);
	}
	if (CandidateWaters.Num() == 0)
	{
		SetComponentTickEnabled(false);
	}
	return false;
}

void USwimDetectionComponent::SetSwimming(bool bEnable)
{
	UCharacterMovementComponent* CMC = OwnerCharacter->GetCharacterMovement();
	if (!CMC) { return; }

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerCharacter);
	bSwimming = bEnable;
	
	if (bEnable)
	{
		// MOVE_Flying으로 바꾸기 전에 진입 직전 상태를 보존한다.
		bSwimEntryFromFall = CMC->MovementMode == MOVE_Falling;

		if (ASC)
		{
			ASC->SetLooseGameplayTagCount(RetrieveGameplayTags::State_Player_Swimming, 1);

			if (URetrieveAbilitySystemComponent* RetrieveASC = Cast<URetrieveAbilitySystemComponent>(ASC))
			{
				RetrieveASC->ClearAbilityInput();
			}

			FGameplayTagContainer PlayerAbilityTags;
			PlayerAbilityTags.AddTag(RetrieveGameplayTags::Ability_Player);
			ASC->CancelAbilities(&PlayerAbilityTags);
		}

		if (UCombatStanceComponent* CombatStance = OwnerCharacter->FindComponentByClass<UCombatStanceComponent>())
		{
			CombatStance->ForceSheatheWeapon();
		}

		// Flying = 중력 off + 자유 3D + 수면 상향 클램프 없음(MOVE_Swimming 한계 회피).
		CMC->SetMovementMode(MOVE_Flying);
		
		if (URetrieveCharacterMovementComponent* RetrieveCMC = Cast<URetrieveCharacterMovementComponent>(CMC))
		{
			RetrieveCMC->NotifySwimEntry(); // 착수 모멘텀 보존, 수면Z 전달 등
		}
	}
	else
	{
		// 이탈 → Falling. 엔진이 작은 간격을 중력으로 부드럽게 착지(직접 Walking은 FloatOffset 높이서 순간이동=스냅).
		// "공짜 착륙 점프"는 CanAttemptJump 완전잠수 게이트로 무력화됨 → 되살려도 안전.
		CMC->SetMovementMode(MOVE_Falling);
		VerticalInput = 0.f;
		if (ASC)
		{
			ASC->SetLooseGameplayTagCount(RetrieveGameplayTags::State_Player_Swimming, 0);
			ASC->SetLooseGameplayTagCount(RetrieveGameplayTags::State_Player_Swimming_UnderWater, 0);
			ASC->SetLooseGameplayTagCount(RetrieveGameplayTags::State_Player_Crouching, 0); // 이탈 시 기립
		}
	}
}
