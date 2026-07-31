#pragma once

#include "CoreMinimal.h"
#include "Components/PawnComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayTagContainer.h"
#include "CombatStanceComponent.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
struct FEnemyPlayerSpottedPayload;

UENUM(BlueprintType)
enum class ERetrieveCombatStance : uint8 {
	Sheathed,		// 납검
	DrawnRelaxed,	// 발검 + 평상
	DrawnCombat,	// 발검 + 전투
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RETRIEVE_API UCombatStanceComponent : public UPawnComponent
{
	GENERATED_BODY()

public:
	UCombatStanceComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// 전투 태세(Combat) 진입점 — 적 포착(PlayerSpotted) 전용. 그 외 발검 활동은 NotifyDrawnActivity로.
	// bFromAttack=true면 발검 몽타주 없이 즉시 손 소켓 스냅.
	UFUNCTION(BlueprintCallable, Category = "Retrieve|CombatStance")
	void NotifyCombatActivity(bool bFromAttack = false);

	// 발검(Relaxed) 진입점 — 장착/공격/조준이 호출. 납검이면 발검으로 올리고, 이미 발검이면 유지(Combat 강등 없음).
	// bInstant=true면 발검 몽타주 스킵 + 소켓 즉시 스냅(장착·공격發 발검).
	UFUNCTION(BlueprintCallable, Category = "Retrieve|CombatStance")
	void NotifyDrawnActivity(bool bInstant = false);

	// 수영 입수처럼 전투 연출을 재생할 수 없는 상황에서 즉시 납검한다.
	void ForceSheatheWeapon();
	
	// ASC 연결 - 캐릭터의 ASC를 초기화시 호출(아래 1 ~ 4)
    void InitializeWithAbilitySystem(UAbilitySystemComponent* ASC);
	void UninitializeFromAbilitySystem();
	
protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|CombatStance", meta = (ClampMin = "0.0"))
	float RelaxDelay = 3.5f; // Combat -> Relaxed

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|CombatStance", meta = (ClampMin = "0.0"))
	float SheatheDelay = 5.f; // Relaxed -> Sheathed
	
private:
	// bInstant=true면 연출(몽타주) 없이 소켓을 즉시 스왑(공격전 발검 / 수영 입수 등). false면 몽타주 GA 트리거.
	void SetStance(ERetrieveCombatStance NewStance, bool bInstant = false);
	bool IsPlayerAttacking() const;
	bool IsAiming() const;
	void HandleRelaxTimer();
	void HandleSheatheTimer();
	void HandleAbilityActivated(UGameplayAbility* Ability);

	// 무기 메시 스폰 직후 호출 — 새로 스폰된 무기를 현재 스탠스(손/등)에 맞춘다.
	// (런타임 인벤토리 장착이 스탠스 init보다 늦어도 소켓이 어긋나지 않게 보장)
	void HandleWeaponVisualsSpawned();

	// 무기 장착 시(스폰 전) 호출 — 장착을 '발검 활동'으로 취급해 스탠스를 발검으로 승격한다.
	// 연출은 Equip 몽타주가 담당하므로 발검 몽타주는 스킵(bFromAttack=true). 스폰 직후 손 소켓에 안착.
	UFUNCTION()
	void HandleWeaponEquipped(FName WeaponItemId);

	// 전투 시작 시 Combat 태그 처리
	void HandleLocalCombatContextChanged(bool bEngaged);
	
	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> OwnerASC;

	ERetrieveCombatStance CurrentStance = ERetrieveCombatStance::Sheathed;

	FTimerHandle RelaxTimerHandle;
	FTimerHandle SheatheTimerHandle;
	FDelegateHandle AbilityActivateHandle;
	FDelegateHandle WeaponVisualsSpawnedHandle;
	FDelegateHandle CombatContextChangedHandle;
};
