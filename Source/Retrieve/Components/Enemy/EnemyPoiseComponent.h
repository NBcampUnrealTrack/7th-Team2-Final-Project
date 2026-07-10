#pragma once

#include "CoreMinimal.h"
#include "Components/PawnComponent.h"
#include "EnemyPoiseComponent.generated.h"

class URetrieveAbilitySystemComponent;
struct FGameplayEventData;
struct FMonsterDataRow;

UCLASS(ClassGroup = "Retrieve", meta = (BlueprintSpawnableComponent))
class RETRIEVE_API UEnemyPoiseComponent : public UPawnComponent
{
	GENERATED_BODY()

public:
	UEnemyPoiseComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void InitializeFromMonsterData(const FMonsterDataRow& Row, bool bIgnorePendingPoiseDamage = false);

	/** 재조우(리스폰/재활성) 시 포이즈·그로기 쿨다운을 초기화한다. */
	void ResetRespawnState();

private:
	void OnAbilitySystemInitialized();
	void HandleHitEvent(const FGameplayEventData* Payload);
	void ApplyPoiseDamage(float DamageDone, AActor* Instigator);
	void ResetPoise() const;
	void TryTriggerGroggy(AActor* Instigator);

	URetrieveAbilitySystemComponent* GetASC() const;
	bool IsPoiseEnabled() const;

	float MaxPoise = 0.f;
	float PoiseDamageMultiplier = 1.f;
	float PoiseGroggyDuration = 3.f;
	float GroggyCooldown = 10.f;
	float NextGroggyAllowedTime = 0.f;
	bool bIgnoreNextPoiseDamage = false;

	FDelegateHandle HitNormalEventHandle;
	FDelegateHandle HitHeavyEventHandle;
};
