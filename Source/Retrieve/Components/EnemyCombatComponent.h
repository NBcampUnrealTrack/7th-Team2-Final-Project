#pragma once

#include "CoreMinimal.h"
#include "Components/PawnComponent.h"
#include "EnemyCombatComponent.generated.h"

class UDataTable;
class URetrieveAbilitySystemComponent;
class USphereComponent;
class UGameplayEffect;

struct FMonsterPatternRow;
struct FGameplayTag;

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

	bool RequestBasicAttack(AActor* Target);
	
	bool RequestPatternByPriority(AActor* Target);
	
	bool HasAvailablePatternByType(AActor* Target, FGameplayTag PatternType) const;
	
	void StopCurrentPattern();

	bool IsPatternActive() const;
	
	bool IsAttackable() const;
	
	FName GetActivePatternRowName() const { return ActivePatternRowName; }

	UDataTable* GetPatternTable() const { return PatternTable.Get(); }

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Components|Pattern")
	void ActivateHitbox();
	
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Components|Pattern")
	void DeactivateHitbox();
	
	void SetActiveHitbox(USphereComponent* NewHitbox);
	
private:
	UFUNCTION()
	void OnHitboxOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
						 UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
						 bool bFromSweep, const FHitResult& SweepResult);
	
	const FMonsterPatternRow* FindBestPattern(AActor* Target, FGameplayTag RequiredPatternType
		, FName* OutRowName = nullptr) const;
	bool IsCooldownReady(FName RowName) const;
	void StartCooldown(FName RowName, float Duration);
	URetrieveAbilitySystemComponent* GetASC() const;

	UPROPERTY()
	TObjectPtr<UDataTable> PatternTable;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Combat|BasicAttack")
	FName BasicAttackRowName;
	
	UPROPERTY(VisibleAnywhere)
	FName ActivePatternRowName;
	
	TArray<FName> PatternSlots;

	TMap<FName, float> CooldownExpiry;

	
	UPROPERTY()
	TObjectPtr<USphereComponent> ActiveHitboxComp;

	/** 히트박스 적중 시 적용할 GE (임시: GE_DamageTest, 추후 DT 연동으로 교체) */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Combat|Hitbox")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	TSet<TWeakObjectPtr<AActor>> HitActors;
};
