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

	// 모든 전투 트리거의 단일 진입점(공격/피격/락온/2차 적 어그로가 전부 호출).
	// bFromAttack=true면 '공격전 발검' → 발검 몽타주 없이 즉시 손 소켓 스냅(공격 모션이 발검을 표현).
	UFUNCTION(BlueprintCallable, Category = "Retrieve|CombatStance")
	void NotifyCombatActivity(bool bFromAttack = false);
	
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
	void HandleRelaxTimer();
	void HandleSheatheTimer();
	void HandleAbilityActivated(UGameplayAbility* Ability);

	// 적이 나를 포착(Channel.Enemy.PlayerSpotted)하면 전투 태세 진입. AI는 이미 이 메시지를 쏘므로 무수정.
	// 현재 1티어(포착→진입). Lost 신호가 생기면 위협집합 기반 정밀 해제 + 의심/교전 2티어로 확장(시드).
	void HandlePlayerSpotted(FGameplayTag Channel, const FEnemyPlayerSpottedPayload& Payload);

	// 무기 메시 스폰 직후 호출 — 새로 스폰된 무기를 현재 스탠스(손/등)에 맞춘다.
	// (런타임 인벤토리 장착이 스탠스 init보다 늦어도 소켓이 어긋나지 않게 보장)
	void HandleWeaponVisualsSpawned();

	// 무기 장착 시(스폰 전) 호출 — 장착을 '발검 활동'으로 취급해 스탠스를 발검으로 승격한다.
	// 연출은 Equip 몽타주가 담당하므로 발검 몽타주는 스킵(bFromAttack=true). 스폰 직후 손 소켓에 안착.
	UFUNCTION()
	void HandleWeaponEquipped(FName WeaponItemId);

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> OwnerASC;

	ERetrieveCombatStance CurrentStance = ERetrieveCombatStance::Sheathed;

	FTimerHandle RelaxTimerHandle;
	FTimerHandle SheatheTimerHandle;
	FDelegateHandle AbilityActivateHandle;
	FDelegateHandle WeaponVisualsSpawnedHandle;
	FGameplayMessageListenerHandle SpottedListenerHandle;
};
