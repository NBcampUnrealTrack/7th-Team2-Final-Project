#pragma once

#include "CoreMinimal.h"
#include "Components/PawnComponent.h"
#include "BossPhaseComponent.generated.h"

class UDataTable;

/**
 * 보스 HP 임계값을 감시하고 페이즈 전환을 처리합니다.
 *
 * - ARetrieveBossCharacter::InitializeComponents()에서 Initialize() 호출.
 * - DT_BossStats(FBossStatsRow)에서 PhaseCount / Phase2/3HPThreshold를 읽습니다.
 * - 임계값 도달 시:
 *     1. 오너의 MonsterDataRowName을 다음 페이즈 행으로 갱신
 *     2. EnemyCombatComponent 패턴 슬롯 재초기화
 *     3. GameplayEvent.Boss.PhaseTransition ASC 전송
 *
 * 페이즈 행 이름 규칙: {BossStatsRowName}_Phase{N}
 *   예) BossStatsRowName = "Boss_Fire"  →  "Boss_Fire_Phase2"
 */
UCLASS(ClassGroup="Retrieve", meta=(BlueprintSpawnableComponent))
class RETRIEVE_API UBossPhaseComponent : public UPawnComponent
{
	GENERATED_BODY()

public:
	/**
	 * ARetrieveBossCharacter::InitializeComponents()에서 호출.
	 * @param InBossStatsTable   DT_BossStats 테이블
	 * @param InBossStatsRowName DT_BossStats 행 이름 (예: "Boss_Fire")
	 * @param InMonsterDataTable DT_MonsterData 테이블 (페이즈 전환 시 패턴 슬롯 갱신용)
	 */
	void Initialize(UDataTable* InBossStatsTable, FName InBossStatsRowName,
	                UDataTable* InMonsterDataTable);

	UFUNCTION(BlueprintPure, Category="Retrieve|Boss")
	int32 GetCurrentPhase() const { return CurrentPhase; }

private:
	UFUNCTION()
	void OnHealthChanged(float NewHealth);

	void TransitionToNextPhase();

	UPROPERTY()
	TObjectPtr<UDataTable> BossStatsTable;

	UPROPERTY()
	TObjectPtr<UDataTable> MonsterDataTable;

	FName  BossStatsRowName;
	int32  CurrentPhase        = 1;
	int32  MaxPhases           = 1;
	float  Phase2HPThreshold   = 0.5f;
	float  Phase3HPThreshold   = 0.25f;
};
