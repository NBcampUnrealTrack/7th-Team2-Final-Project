#pragma once

#include "CoreMinimal.h"
#include "Character/RetrieveEnemyCharacter.h"
#include "RetrieveBossCharacter.generated.h"

class UBossPhaseComponent;
struct FBossStatsRow;
struct FMonsterDataRow;

/**
 * 보스 캐릭터 베이스 클래스 (가디언 3종 + 여왕).
 *
 * ARetrieveEnemyCharacter를 상속하여 ASC·HealthComponent·EnemyCombatComponent 등을
 * 그대로 재사용하고, 아래 요소를 추가합니다.
 *
 *  - UBossPhaseComponent  : HP 임계값 → DT_MonsterData 행 교체 + EnemyCombatComponent 갱신
 *  - DT_BossStats 참조    : PhaseCount / HPThreshold / GroggyDuration / UnlockElementTag
 *  - HandleDeathStarted   : State.Boss.Dead 적용, GameplayEvent.Boss.Die 전송,
 *                           가디언 → Channel.Quest.GuardianDefeated
 *                           여왕   → Channel.Game.QueenDefeated
 *
 * 에디터에서 반드시 설정할 항목:
 *   - MonsterDataRowName  (예: "Boss_Fire_Phase1")  — 상속된 필드
 *   - MonsterDataTable    (DT_MonsterData)           — 상속된 필드
 *   - PatternTable        (DT_MonsterPattern)        — 상속된 필드
 *   - BossStatsRowName    (예: "Boss_Fire")
 *   - BossStatsTable      (DT_BossStats)
 */
UCLASS()
class RETRIEVE_API ARetrieveBossCharacter : public ARetrieveEnemyCharacter
{
	GENERATED_BODY()

public:
	ARetrieveBossCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * UBossPhaseComponent가 페이즈 전환 시 호출합니다.
	 * MonsterDataRowName 갱신 + EnemyCombatComponent 패턴 슬롯 재초기화.
	 */
	void UpdateMonsterDataRow(FName NewRow);
	
	/** 처치 시 해방되는 원소(여왕/비가디언 보스의 경우 비어있음) Getter.
	 *  UGuardianCoreSpawnerComponent (Channel.Monster.Died 구독자)가 코어 스폰 분기를 위해 읽습니다. */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Boss")
	FGameplayTag GetUnlockElementTag() const; 

	void SetIntroState(bool bEnabled);
	
protected:
	virtual void InitializeComponents() override;
	virtual void HandleDeathStarted(AActor* OwningActor) override;

	/** 공통 전투상태 리셋에 더해 보스 페이즈를 1페이즈로 되돌린다. */
	virtual void ResetRespawnState() override;

	const FBossStatsRow* GetBossStatsRow() const;
	
protected:
	/** HP 임계값 감시 및 페이즈 전환 처리 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Retrieve|Boss")
	TObjectPtr<UBossPhaseComponent> BossPhaseComponent;

	/** DT_BossStats 행 이름 (예: "Boss_Fire") */
	UPROPERTY(EditDefaultsOnly, Category="Retrieve|Boss")
	FName BossStatsRowName;

	/** DT_BossStats 테이블 */
	UPROPERTY(EditDefaultsOnly, Category="Retrieve|Boss")
	TObjectPtr<UDataTable> BossStatsTable;
};
