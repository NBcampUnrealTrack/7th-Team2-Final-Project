#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "Combat/RetrieveCombatTypes.h"
#include "GameplayTagContainer.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "Character/Cosmetics/RetrieveBowMontageSet.h"
#include "GA_BowShot.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitInputRelease;
class UAbilityTask_WaitGameplayTagRemoved;
class UAnimMontage;
class UGameplayEffect;
class UWeaponComponent;
class AStaffProjectile;
class URetrieveBowLinkedAnimInstance;
class URetrieveBowMeshAnimInstance;
// EBowShotPhase는 RetrieveBowMontageSet.h에 정의(캐릭터·활메시 공용).

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
	void StartCharging();               // 진입: 장전 필요 판정 게이트
	void BeginCharge();                 // 실제 차징 시작(릴리즈 감시/드로우/타이머)
	void PlayReloadThenBeginCharge();   // 미노킹 시 장전 몽타주 → 완료 후 BeginCharge

	UFUNCTION()
	void HandleReloadFinished();

	UFUNCTION()
	void HandleChargeReleased(float TimeHeld);

	UFUNCTION()
	void HandleAimTagRemoved();

	// 인트로(DrawnStart) 블렌드아웃 → 홀드 루프(Drawn) 진입.
	UFUNCTION()
	void PlayChargeHold();

	// 풀차지 도달(타이머) → 손떨림 루프(DrawnShake)로 교체.
	void HandleFullChargeReached();

	// 차징 루프 몽타주 교체 재생(기존 차징 태스크 종료 후 새로 재생).
	void StartChargeLoopMontage(UAnimMontage* Montage);

	// 차징 상태 종료: bCharging=false + 차징 몽타주/풀차지 타이머 정지.
	void StopCharging();

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
	// 캐릭터 레이어에서 phase 몽타주를 자세에 맞게 해결. 레이어 없으면 nullptr → 호출부 스킵.
	UAnimMontage* ResolveShotMontage(EBowShotPhase Phase) const;
	// 활 메시 AnimInstance에 phase 몽타주를 재생(캐릭터와 lockstep). 미설정 시 무동작.
	// 루프는 몽타주 애셋 자체에 맡긴다(코드 강제 루프 없음).
	void PlayBowMeshMontage(EBowShotPhase Phase, float PlayRate);
	bool IsOwnerCrouched() const;

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
	// 차징/발사 몽타주는 활별 레이어(URetrieveBowLinkedAnimInstance)가 소유한다.
	// GA는 장착 시 링크된 레이어를 CachedBowLayer로 잡아 단계별로 꺼내 재생한다.
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

	// 장착된 활의 링크 레이어. 캐릭터 차징/발사 몽타주 소스. ActivateAbility에서 캐시.
	TWeakObjectPtr<URetrieveBowLinkedAnimInstance> CachedBowLayer;

	// 활 메시의 AnimInstance. 활메시 phase 몽타주 소스+재생 대상(캐릭터와 lockstep). ActivateAbility에서 캐시.
	TWeakObjectPtr<URetrieveBowMeshAnimInstance> CachedBowMeshAnim;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	// 차징 인트로/홀드/셰이크 재생용(발사 MontageTask와 분리).
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> ChargeMontageTask;

	// 빈 활 장전(Reload) 재생용. 완료 시 BeginCharge로 이어진다.
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> ReloadMontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitInputRelease> ChargeReleaseTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayTagRemoved> AimTagRemovedTask;

	TArray<FTimerHandle> SpawnTimerHandles;

	// 풀차지 도달 시각 트리거(홀드 → 셰이크 교체).
	FTimerHandle FullChargeTimerHandle;

	// 차징 구간 여부. 릴리즈/조준해제 후 늦게 도착한 차징 콜백을 무시하는 가드.
	bool bCharging = false;

	FGameplayTag CachedElementTag;
	float CachedChargeMultiplier = 1.f;
	float CachedChargeRatio = 1.f;
	float CachedEmpowerMultiplier = 1.f;
};