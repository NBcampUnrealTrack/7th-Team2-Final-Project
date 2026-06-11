
#include "SwimDetectionComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Interface/RetrieveWaterProvider.h"

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
	const float SurfaceZ = IRetrieveWaterProvider::Execute_GetWaterSurfaceZ(CurrentWater.GetObject(), Loc);
	const float DepthDiff = SurfaceZ - Loc.Z;

	// 깊이 게이트(히스테리시스)로 우리가 직접 모드 전환. 박스 물엔 PhysicsVolume 자동전환이 없음.
	if (!bSwimming && DepthDiff > ChestThreshold)
	{
		SetSwimming(true);
	}
	else if (bSwimming && DepthDiff < WadeThreshold)
	{
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
				(DepthDiff > UnderwaterDepthThreshold) ? 1 : 0);
		}

		// 상하 트림 (입력 레이어가 의도 전달, 적용은 여기 — Hero는 tick 없음)
		if (VerticalInput != 0.f)
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
		// 입수 모멘텀 클램프: 점프/낙하 가속이 저항 없는 수영에서 폭주하는 것 방지(진입 1회).
		CMC->Velocity = CMC->Velocity.GetClampedToMaxSize(CMC->GetMaxSpeed());
		if (ASC)
		{
			ASC->SetLooseGameplayTagCount(RetrieveGameplayTags::State_Player_Swimming, 1);
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
		}
	}
}
