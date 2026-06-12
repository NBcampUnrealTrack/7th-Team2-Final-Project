
#include "SwimDetectionComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/CapsuleComponent.h"
#include "Components/RetrieveCharacterMovementComponent.h"
#include "Engine/Engine.h"
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
	if (!OwnerCharacter || !CurrentWater.GetObject())
	{
		return;
	}

	UCharacterMovementComponent* CMC = OwnerCharacter->GetCharacterMovement();
	if (!CMC) { return; }

	// 수면 깊이 = 수면Z - 액터Z (Provider가 평면 상수 / 강 스플라인 계산)
	const FVector Loc = OwnerCharacter->GetActorLocation();
	float SurfaceZ = 0.f;
	const bool bInColumn = IRetrieveWaterProvider::Execute_TryGetWaterColumn(CurrentWater.GetObject(), Loc, SurfaceZ);
	const float DepthDiff = bInColumn ? (SurfaceZ - Loc.Z) : -UE_BIG_NUMBER;
	const URetrieveSwimSettings* Swim = GetDefault<URetrieveSwimSettings>();

	// 깊이 게이트(히스테리시스)로 우리가 직접 모드 전환. 박스 물엔 PhysicsVolume 자동전환이 없음.
	if (!bSwimming && DepthDiff > Swim->ChestThreshold)
	{
		SetSwimming(true);
	}
	else if (bSwimming && (!bInColumn || (DepthDiff < Swim->WadeThreshold && HasFloorBelow())))
	{
		// [임시 디버그] 이탈 유발 조건 확인용 — 해결 시 제거
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red,
				FString::Printf(TEXT("SWIM EXIT: bInColumn=%d DepthDiff=%.1f Floor=%d"),
					bInColumn, DepthDiff, HasFloorBelow() ? 1 : 0));
		}
		SetSwimming(false);
		return;
	}

	if (bSwimming)
	{
		// 수중 모드: 깊으면 UnderWater ON, 얕으면 OFF (깊이 자동)
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerCharacter))
		{
			ASC->SetLooseGameplayTagCount(
				RetrieveGameplayTags::State_Player_Swimming_UnderWater,
				(DepthDiff > Swim->UnderwaterDepthThreshold) ? 1 : 0);

			if (DepthDiff > Swim->UnderwaterDepthThreshold) // 수중 진입 시 Sprint 해제(표면 자유형만)
			{
				ASC->SetLooseGameplayTagCount(RetrieveGameplayTags::State_Player_Sprinting, 0);
			}
		}

		bool bPlunging = false;
		if (URetrieveCharacterMovementComponent* RetrieveCMC = Cast<URetrieveCharacterMovementComponent>(CMC))
		{
			RetrieveCMC->SetWaterSurfaceZ(SurfaceZ); // 부력 계산은 CMC가 담당
			bPlunging = RetrieveCMC->IsPlunging();
		}

		// 상하 트림. 플런지 중엔 입력 무시(순수 모멘텀).
		if (VerticalInput != 0.f && !bPlunging)
		{
			OwnerCharacter->AddMovementInput(FVector::UpVector, VerticalInput);
		}
	}
}

void USwimDetectionComponent::NotifyEnterWaterRegion(const TScriptInterface<IRetrieveWaterProvider>& InWater)
{
	CurrentWater = InWater;
	SetComponentTickEnabled(true);
}

void USwimDetectionComponent::NotifyExitWaterRegion()
{
	if (bSwimming)
	{
		SetSwimming(false);
	}
	CurrentWater = TScriptInterface<IRetrieveWaterProvider>();
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
	if (!OwnerCharacter || CurrentWater.GetObject()) // 이미 진입했으면 skip(멱등)
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
				return;
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

void USwimDetectionComponent::SetSwimming(bool bEnable)
{
	UCharacterMovementComponent* CMC = OwnerCharacter->GetCharacterMovement();
	if (!CMC) { return; }

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerCharacter);
	bSwimming = bEnable;
	
	if (bEnable)
	{
		// Flying = 중력 off + 자유 3D + 수면 상향 클램프 없음(MOVE_Swimming 한계 회피).
		CMC->SetMovementMode(MOVE_Flying);
		if (ASC)
		{
			ASC->SetLooseGameplayTagCount(RetrieveGameplayTags::State_Player_Swimming, 1);
		}
		
		if (URetrieveCharacterMovementComponent* RetrieveCMC = Cast<URetrieveCharacterMovementComponent>(CMC))
		{
			RetrieveCMC->NotifySwimEntry(); // 착수 모멘텀 보존, 수면Z 전달 등
		}
	}
	else
	{
		CMC->SetMovementMode(MOVE_Falling); // 이탈 -> 낙하, 엔진이 착지 시 Walking 해소.
		VerticalInput = 0.f;
		if (ASC)
		{
			ASC->SetLooseGameplayTagCount(RetrieveGameplayTags::State_Player_Swimming, 0);
			ASC->SetLooseGameplayTagCount(RetrieveGameplayTags::State_Player_Swimming_UnderWater, 0);
			ASC->SetLooseGameplayTagCount(RetrieveGameplayTags::State_Player_Crouching, 0); // 이탈 시 기립
		}
	}
}
