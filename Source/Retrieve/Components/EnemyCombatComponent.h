#pragma once

#include "CoreMinimal.h"
#include "Components/PawnComponent.h"
#include "EnemyCombatComponent.generated.h"

class UDataTable;
class URetrieveAbilitySystemComponent;
struct FMonsterPatternRow;

/**
 * 적의 공격 패턴 선택·발동·쿨다운을 담당한다.
 * StateTree Task(FStateTreeTask_EnemyAttack)와 1:1 협력.
 * - EnterState → RequestPatternByPriority
 * - Tick       → IsPatternActive
 * - ExitState  → StopCurrentPattern
 *
 * UPatternCounterComponent가 같은 Pawn에 있으면 패턴 선택 후 자동으로 알린다.
 */
UCLASS(ClassGroup = "Retrieve", meta = (BlueprintSpawnableComponent))
class RETRIEVE_API UEnemyCombatComponent : public UPawnComponent
{
	GENERATED_BODY()

public:
	/** AEnemyCharacter::BeginPlay에서 DT_MonsterData 파싱 후 호출 */
	void Initialize(UDataTable* InPatternTable, const TArray<FName>& InPatternSlots);

	/**
	 * StateTree Task::EnterState에서 호출.
	 * 범위·쿨다운을 통과한 최고 우선순위 패턴을 선택해 GA를 발동한다.
	 * @return 발동 가능한 패턴이 있으면 true, 없으면 false (Task → Failed)
	 */
	bool RequestPatternByPriority(AActor* Target);

	void StopCurrentPattern();

	bool IsPatternActive() const;

	FName GetActivePatternRowName() const { return ActivePatternRowName; }

	UDataTable* GetPatternTable() const { return PatternTable.Get(); }

private:
	const FMonsterPatternRow* FindBestPattern(AActor* Target) const;
	bool IsCooldownReady(FName RowName) const;
	void StartCooldown(FName RowName, float Duration);
	URetrieveAbilitySystemComponent* GetASC() const;

	UPROPERTY()
	TObjectPtr<UDataTable> PatternTable;

	TArray<FName> PatternSlots;

	TMap<FName, float> CooldownExpiry;

	FName ActivePatternRowName;
};
