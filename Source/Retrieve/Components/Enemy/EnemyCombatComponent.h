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
	
	bool RequestPatternByPriority(AActor* Target, FGameplayTag RequiredPatternType);
	
	bool HasAvailablePatternByType(AActor* Target, FGameplayTag PatternType) const;

	bool HasPatternInRangeByTypeIgnoringCooldown(AActor* Target, FGameplayTag PatternType) const;
	
	void StopCurrentPattern();

	bool IsPatternActive() const;
	
	bool IsAttackable(AActor* Target) const;

	bool IsSpecialAttackEvaluationLocked() const;
	bool IsSpecialAttackRetryCooldownReady() const;
	void StartSpecialAttackRetryCooldown();
	void SuppressSpecialAttackEvaluation(float Duration);
	
	void SetMovementLockedByAttack(bool bLocked);
	bool IsMovementLockedByAttack() const { return bMovementLockedByAttack; }

	void SetFocusTarget(AActor* Target);
	AActor* GetFocusTarget() const;
	void ClearFocusTarget();
	void FaceFocusTarget(float DeltaTime, float InterpSpeed = 10.f, bool bYawOnly = true);
	void FaceActor(AActor* Target, float DeltaTime, float InterpSpeed = 10.f, bool bYawOnly = true);
	
	UFUNCTION(BlueprintPure, Category = "Retrieve|Combat|AttackSpeed")
	float GetAttackSpeedMultiplier() const;
	
	UFUNCTION(BlueprintPure, Category = "Retrieve|Combat|AttackSpeed")
	float GetAttackMontagePlayRate(float BasePlayRate = 1.f) const;
	
	UFUNCTION(BlueprintPure, Category = "Retrieve|Combat|AttackSpeed")
	float GetAttackDelay(float BaseDelay, float MinDelay = 0.f) const;
	
	FName GetActivePatternRowName() const { return ActivePatternRowName; }

	UDataTable* GetPatternTable() const { return PatternTable.Get(); }

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Components|Pattern")
	void ActivateHitbox();
	
	void ActivateHitbox(FName InBoneName, FVector InOffset, float InRadius);
	
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Components|Pattern")
	void DeactivateHitbox();
	
	void SetActiveHitbox(USphereComponent* NewHitbox);
	
private:
	UFUNCTION()
	void OnHitboxOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
						 UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
						 bool bFromSweep, const FHitResult& SweepResult);
	bool ApplyHitToActor(AActor* OtherActor, const FHitResult& SweepResult);
	
	const FMonsterPatternRow* FindBestPattern(AActor* Target, FGameplayTag RequiredPatternType
		, FName* OutRowName = nullptr, bool bIgnoreCooldown = false) const;
	bool IsCooldownReady(FName RowName) const;
	void StartCooldown(FName RowName, float Duration);
	void LockSpecialAttackEvaluation(float Duration);
	URetrieveAbilitySystemComponent* GetASC() const;
	bool TryStartSequencePattern(const FMonsterPatternRow& PatternRow, FName PatternRowName, AActor* Target);
	void FinishSequencePattern();
	void SetSequenceAttackTag(bool bEnable);
	void FaceTarget(AActor* Target) const;

private:
	UPROPERTY()
	TObjectPtr<UDataTable> PatternTable;

	UPROPERTY(VisibleAnywhere)
	FName ActivePatternRowName;
	
	TArray<FName> PatternSlots;

	TMap<FName, float> CooldownExpiry;

	bool bSequencePatternActive = false;
	bool bSequenceAttackTagApplied = false;
	FTimerHandle SequenceHitboxStartTimerHandle;
	FTimerHandle SequenceHitboxEndTimerHandle;
	FTimerHandle SequenceFinishTimerHandle;

	
	UPROPERTY()
	TObjectPtr<USphereComponent> ActiveHitboxComp;

	/** 히트박스 적중 시 적용할 데미지 GE. BP에서 지정(예: BP_BossBase = GE_BasicHitDamage). 패턴 행 bCanBeParried면 Attack.Type.Parryable 동적 주입. */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Combat|Hitbox")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	TSet<TWeakObjectPtr<AActor>> HitActors;

	UPROPERTY()
	TWeakObjectPtr<AActor> FocusTarget;
	
	bool bMovementLockedByAttack = false;

	float SpecialAttackEvaluationLockUntilTime = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Combat|SpecialAttack", meta = (ClampMin = "0.0"))
	float SpecialAttackEvaluationLockDuration = 0.5f;

	float SpecialAttackRetryCooldownUntilTime = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Combat|SpecialAttack", meta = (ClampMin = "0.0"))
	float SpecialAttackRetryCooldownDuration = 1.5f;
};
