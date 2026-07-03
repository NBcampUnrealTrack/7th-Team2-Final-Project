#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "Combat/RetrieveCombatTypes.h"
#include "GameplayTagContainer.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "GA_BowShot.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitInputRelease;
class UAbilityTask_WaitGameplayTagRemoved;
class UAnimMontage;
class UGameplayEffect;
class UWeaponComponent;
class AStaffProjectile;

/**
 * 활(궁수) 조준 사격 — 좌클릭 홀드로 차징, 릴리즈 시점 차징량으로 데미지 스케일 후 화살 발사.
 * 우클릭 조준(State.Player.Aiming) 해제 시 즉시 캔슬. 카메라 트레이스 조준점으로 아크 발사(포물선).
 */
UCLASS()
class RETRIEVE_API UGA_BowShot : public URetrieveGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_BowShot();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	// ---- 차징 흐름 ----
	void StartCharging();

	UFUNCTION()
	void HandleChargeReleased(float TimeHeld);

	UFUNCTION()
	void HandleAimTagRemoved();

	// ---- 발사 ----
	void ScheduleProjectiles();
	void SpawnProjectile();
	void PlayFireMontageThenEnd();

	UFUNCTION()
	void HandleMontageFinished();

	// ---- 헬퍼 ----
	float ComputeChargeDamageMultiplier(float TimeHeld) const;
	// 크로스헤어 방향의 "임계거리" 지점을 반환(차징으로 거리 스케일). 이 지점으로 아크가 교차한다.
	FVector ComputeAimPoint() const;
	// 발사당 소비 화살 수 = 투사체 수(멀티샷 대응).
	int32 GetArrowCost() const;
	// 차징 상태를 로컬 UI(레티클)로 브로드캐스트.
	void BroadcastChargeState(ERetrieveBowChargePhase Phase, float ChargeRatio) const;

private:
	// ---- 발사체 ----
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow")
	TSubclassOf<AStaffProjectile> ProjectileClass;

	// 풀차징 발사 속도. 약차징은 MinChargeProjectileSpeed까지 선형 감소.
	// 아크·임계사거리가 보이도록 낮게 잡는다(빠르면 궤적이 평평해 미사일이 됨).
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow", meta = (ClampMin = "0.0"))
	float ProjectileSpeed = 4500.f;

	// 무차징(0초) 발사 속도. 차징 비율(0~1)로 ProjectileSpeed까지 보간.
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow", meta = (ClampMin = "0.0"))
	float MinChargeProjectileSpeed = 2000.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow")
	TArray<float> FireDelays;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow", meta = (ClampMin = "0.0"))
	float DamageMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow")
	ERetrieveHitReactType HitReactType = ERetrieveHitReactType::Flinch;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow")
	FName SpawnSocketName = NAME_None;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow")
	FVector SpawnOffset = FVector(40.f, 0.f, 50.f);

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow")
	FGameplayTag ChargeBonusEventTag;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow|Element")
	TMap<FGameplayTag, TSubclassOf<UGameplayEffect>> ElementStatusEffects;

	// ---- 시전 몽타주 ----
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow")
	TSoftObjectPtr<UAnimMontage> FireMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow", meta = (ClampMin = "0.1"))
	float MontagePlayRate = 1.0f;

	// ---- 차징 ----
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow|Charge", meta = (ClampMin = "0.0"))
	float MaxChargeTime = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow|Charge", meta = (ClampMin = "0.0"))
	float MinChargeDamageMultiplier = 0.4f;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow|Charge", meta = (ClampMin = "0.0"))
	float MaxChargeDamageMultiplier = 2.0f;

	// 발사에 필요한 최소 차징 시간(초). 이보다 짧게 떼면 발사 안 됨(취소, 화살 미소비). 0 = 제한 없음.
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow|Charge", meta = (ClampMin = "0.0"))
	float MinChargeTimeToFire = 0.4f;

	// ---- 강화 화살(버스트 훅) ----
	// State.Player.BowShot.Empowered 보유 시 이 배율을 곱하고 태그를 소비.
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow|Empower", meta = (ClampMin = "1.0"))
	float EmpoweredDamageMultiplier = 1.5f;

	// ---- 조준 / 낙차 ----
	// 무차징 영점(임계) 거리. 이 거리에서 궤적이 크로스헤어와 교차하고, 넘어가면 급락한다.
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow|Trajectory", meta = (ClampMin = "0.0"))
	float MinCriticalRange = 750.f;

	// 풀차징 영점(임계) 거리. 차징 비율(0~1)로 MinCriticalRange와 보간 → 임계거리 동적.
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow|Trajectory", meta = (ClampMin = "0.0"))
	float MaxCriticalRange = 3500.f;

	// 0이면 낙차 없음(완전 직선). 1.0이면 월드 중력 그대로. 낮출수록 아케이드에 가깝다.
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow|Trajectory", meta = (ClampMin = "0.0"))
	float ArrowGravityScale = 1.0f;

	// ---- 화살 재고(인벤토리 소비 훅) ----
	// ArrowItemId가 None이면 무제한(현행 유지). 설정 시 인벤토리에서 조회/소비한다.
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow|Inventory")
	FName ArrowItemId = NAME_None;

	// RemoveItem에 넘길 카테고리 태그 (예: Item.Consumable, Item.Material)
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow|Inventory", meta = (Categories = "Item"))
	FGameplayTag ArrowItemCategoryTag;

	// ---- 런타임 상태 ----
	UPROPERTY(Transient)
	TObjectPtr<UWeaponComponent> CachedWeaponComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitInputRelease> ChargeReleaseTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayTagRemoved> AimTagRemovedTask;

	TArray<FTimerHandle> SpawnTimerHandles;

	FGameplayTag CachedElementTag;
	float CachedChargeMultiplier = 1.f;
	float CachedChargeRatio = 1.f;
	float CachedEmpowerMultiplier = 1.f;
};