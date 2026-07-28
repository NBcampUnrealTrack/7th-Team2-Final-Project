#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "RetrieveAbilitySystemComponent.generated.h"

class AActor;
class URetrieveGameplayAbility;

/**
 * 버퍼에 적재된 전투 입력 1건.
 * 입력 시점에 기록되고, ResolveBufferedCombatInput()이 소비하거나 시간이 지나면 만료시킨다.
 */
USTRUCT()
struct RETRIEVE_API FRetrieveBufferedCombatInput
{
	GENERATED_BODY()

	// 이 입력이 발동시키려는 어빌리티.
	UPROPERTY()
	FGameplayAbilitySpecHandle AbilitySpecHandle;

	// 입력 의도 태그(= 입력 태그). 캔슬 윈도우의 AllowedCancelIntents와 매칭하는 키.
	UPROPERTY()
	FGameplayTag IntentTag;

	// 적재 시각(월드 초). 만료 판정용.
	double TimeSeconds = 0.0;

	// 유효 시간(초). TimeSeconds + BufferSeconds 가 지나면 폐기된다.
	float BufferSeconds = 0.25f;

	// 소비 우선순위(높을수록 먼저).
	int32 Priority = 0;

	// 적재 순번(동순위일 때 최신 입력 우선 판정용).
	int32 Sequence = 0;
};

/**
 * 입력 → GAS 어빌리티 활성화를 담당하는 ASC.
 *
 * 전투 입력(bUseCombatInputBuffer를 켠 어빌리티):
 *   1) 입력 시 CombatInputBuffer에 '적재만' 한다(즉시 발동하지 않음).
 *   2) 매 프레임 ResolveBufferedCombatInput()이 버퍼를 보고 "지금 발동 가능한 1건"을 발동한다.
 * 비전투 입력(상호작용 등, bUseCombatInputBuffer=false)은 기존처럼 즉시 처리한다.
 *
 * "무엇이 무엇을 캔슬할 수 있는가"는 전적으로 몽타주의 AttackCancelWindow(AllowedCancelIntents)가 정한다.
 * 그 윈도우가 Add/RemoveAttackCancelWindow로 허용 intent를 ref-count 등록/해제하고,
 * 리졸버는 IsAttackCancelIntentAllowed로 조회만 한다.
 */
UCLASS()
class RETRIEVE_API URetrieveAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;

	void AbilityInputTagPressed(const FGameplayTag& InputTag);
	void AbilityInputTagReleased(const FGameplayTag& InputTag);

	/**
	 * 어빌리티가 물리 키를 아직 떼지 않은 상태에서 스스로 조기 종료할 때(예: 토글 해제) 호출한다.
	 * InputHeldSpecHandles에 남아있으면 같은 프레임의 WhileInputActive 재시도가 즉시 재발동시키므로,
	 * 토글 종료 시점에 이 목록에서 미리 빼내 그 재발동을 막는다.
	 */
	void ClearInputHeldForSpec(const FGameplayAbilitySpecHandle& SpecHandle);

	// 이 자산 태그를 가진 스펙을 홀드 목록에서 제거(WhileInputActive 자동 재시도 억제).
	// 입력 바인딩 태그를 몰라도 동작 — 달리기가 Guard/Aim 취소 시 사용.
	void ClearInputHeldForAbilityWithTag(const FGameplayTag& AbilityAssetTag);

	// 이 입력 인텐트로 발동 가능한(grant된) 어빌리티가 하나라도 있는가.
	// chord 등 입력 치환 전에 "치환 대상이 실제로 존재하는지" 확인용(없는 클래스 입력 먹통 방지).
	bool HasActivatableAbilityWithInputTag(const FGameplayTag& InputTag) const;

	// 지금 발동되는 어빌리티가 '진행 중 공격을 끊는 캔슬-인'으로 발동됐는가(리졸버가 발동 직전 기록).
	// CancelOpen 태그는 CancelAbilitiesWithTag로 ActivateAbility 전에 지워져, 캔슬 여부 판정에 못 쓴다.
	bool IsActivatingAsCancel() const { return bActivatingAsCancel; }

	// 매 프레임 호출(비전투 즉시 입력 처리 + 버퍼 소비).
	void ProcessAbilityInput(float DeltaTime, bool bGamePaused);
	void ClearAbilityInput();

	// === 캔슬 윈도우 (AttackCancelWindow ANS가 호출) ===
	// 윈도우가 열려 있는 동안 허용 intent를 ref-count로 등록/해제한다(윈도우 겹침 안전).
	void AddAttackCancelWindow(const FGameplayTag& CancelOpenTag, const FGameplayTagContainer& AllowedCancelIntents);
	void RemoveAttackCancelWindow(const FGameplayTag& CancelOpenTag, const FGameplayTagContainer& AllowedCancelIntents);
	void ClearAttackCancelWindows(const FGameplayTag& CancelOpenTag);
	// 지금 열린 윈도우가 이 intent의 캔슬을 허용하는가.
	bool IsAttackCancelIntentAllowed(const FGameplayTag& IntentTag) const;

	// 카운터 타겟 산출용 약참조 저장/조회. 카운터 성공 시 ParryCounter 대시를 때려박기 위해 UGA_ParryCounter가 사용한다.
	void SetPendingCounterTarget(AActor* InTarget, float WindowDuration = 0.f);
	AActor* GetPendingCounterTarget() const;
	void ClearPendingCounterTarget();

	void SetCounterWarpTargetLocked(bool bInLocked);
	bool IsCounterWarpTargetLocked() const;

protected:
	// 입력 1건을 버퍼에 적재한다(같은 어빌리티+intent는 최신 1건으로 갱신).
	void BufferCombatInput(const FGameplayAbilitySpec& AbilitySpec, const FGameplayTag& InputTag, const URetrieveGameplayAbility& AbilityCDO);
	// 만료된 버퍼 입력을 제거한다.
	void PruneExpiredCombatInputs(double NowSeconds);
	// 버퍼에서 '지금 발동 가능한 1건'을 우선순위 순으로 찾아 소비한다. 이 시스템의 유일한 발동 결정 지점.
	void ResolveBufferedCombatInput();
	// Ability.Type.Attack 자산태그를 가진 어빌리티가 현재 활성인가(= 공격 진행 중인가).
	bool IsAttackAbilityActive() const;
	// 이 자산태그를 가진 어빌리티가 현재 활성인가. (중복 스펙의 콤보 재시작 차단용 — ResolveBufferedCombatInput)
	bool HasActiveAbilityWithAssetTag(const FGameplayTag& AssetTag) const;

	TArray<FGameplayAbilitySpecHandle> InputPressedSpecHandles;
	TArray<FGameplayAbilitySpecHandle> InputReleasedSpecHandles;
	TArray<FGameplayAbilitySpecHandle> InputHeldSpecHandles;

	UPROPERTY(Transient)
	TArray<FRetrieveBufferedCombatInput> CombatInputBuffer;

	// intent별 '현재 열린 캔슬 윈도우 수'(ref-count). count>0 이면 그 intent의 캔슬이 허용된 상태.
	TMap<FGameplayTag, int32> AttackCancelIntentCounts;

	// 적재 순번 카운터(동순위 최신 우선용).
	int32 CombatInputSequence = 0;

	// 리졸버가 캔슬-인으로 발동시키는 동안만 true(TryActivateAbility 직전 set, 직후 clear). 동기 구간 전용.
	bool bActivatingAsCancel = false;

	// 카운터 성공시 Parry Counter 대시를 때려박기 위한 대상을 약참조로 저장
	TWeakObjectPtr<AActor> PendingCounterTarget;

	// 카운터 수용 시간이 지나면 PendingCounterTarget을 자동 소멸시키는 타이머.
	FTimerHandle PendingCounterClearTimer;

	// 카운터 가능 윈도우 동안 아바타(플레이어) 메시에 붉은 아웃라인(커스텀뎁스 스텐실)을 켜고 끈다.
	void SetCounterReadyHighlight(bool bEnabled);

	// 카운터 아웃라인용 커스텀뎁스 스텐실 값(PostProcess 머티리얼이 이 값을 붉게 그린다). 락온(255)과 구분.
	static constexpr int32 CounterReadyStencilValue = 252;

	// 카운터 대시가 직접 등록한 워프 타겟을 RetrieveAttackWarp Notify가 덮어쓰지 않게 막는다.
	bool bCounterWarpTargetLocked = false;
};
