#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "Data/RetrieveDataTableTypes.h"
#include "RetrieveStaminaSettings.generated.h"

/**
 * 플레이어 스태미너 전역 설정. Project Settings > Retrieve > Stamina. 스태미너 튜닝은 전부 여기 한 곳.
 *
 * - 최대치/자연 회복: MaxStamina, RegenPerSecond, RegenDelaySeconds (StaminaComponent가 사용).
 * - 행동별 소모/게이팅/드레인/회복: StaminaCosts 맵(액션 GameplayTag → FStaminaCostRow).
 *   URetrieveGameplayAbility가 자기 StaminaCostTag로 조회해 CheckCost/ApplyCost에 적용.
 * 실제 스태미너 증감은 네이티브 URetrieveStaminaCostEffect(SetByCaller) 하나로 처리한다.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Retrieve Stamina"))
class RETRIEVE_API URetrieveStaminaSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URetrieveStaminaSettings();

	virtual FName GetCategoryName() const override { return TEXT("Retrieve"); }

	// 스태미너 최대치(플레이어). 스폰 시 Stamina/MaxStamina 어트리뷰트 초기값으로 사용된다.
	UPROPERTY(Config, EditAnywhere, Category = "Stamina|Pool", meta = (ClampMin = "1.0"))
	float MaxStamina = 100.f;

	// 전투 중 자연 회복 속도(초당). 소모 후 RegenDelaySeconds가 지나면 이 속도로 회복.
	UPROPERTY(Config, EditAnywhere, Category = "Stamina|Regen", meta = (ClampMin = "0.0"))
	float RegenPerSecond = 35.f;

	// 비전투(탐험) 자연 회복 속도(초당). 전투가 아니면 지연 없이 이 속도로 빠르게 회복한다.
	UPROPERTY(Config, EditAnywhere, Category = "Stamina|Regen", meta = (ClampMin = "0.0"))
	float OutOfCombatRegenPerSecond = 70.f;

	// 소모 후 자연 회복이 재개되기까지의 지연(초). **전투 중에만** 적용(비전투는 지연 없이 즉시 회복).
	// 지연 중 재소모하면 지연이 리셋된다(소울류 감각).
	UPROPERTY(Config, EditAnywhere, Category = "Stamina|Regen", meta = (ClampMin = "0.0"))
	float RegenDelaySeconds = 1.0f;

	// 전투 상태(State.Player.Combat)에서 질주(Sprint) 시 초당 소모. 소진되면 질주가 강제 종료된다.
	// 0이면 전투 질주도 무료. 비전투 질주는 항상 무료(탐험 이동은 소모 없음).
	UPROPERTY(Config, EditAnywhere, Category = "Stamina|Cost", meta = (ClampMin = "0.0"))
	float SprintDrainPerSecond = 12.f;

	// 액션 태그 → 비용. 소모하는 액션만 항목을 두면 되고, 없는 태그는 무료로 동작한다.
	UPROPERTY(Config, EditAnywhere, Category = "Stamina|Cost", meta = (Categories = "Ability", ForceInlineRow))
	TMap<FGameplayTag, FStaminaCostRow> StaminaCosts;
};
