#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "RetrieveAbilitySystemComponent.generated.h"

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

protected:
	// 입력 1건을 버퍼에 적재한다(같은 어빌리티+intent는 최신 1건으로 갱신).
	void BufferCombatInput(const FGameplayAbilitySpec& AbilitySpec, const FGameplayTag& InputTag, const URetrieveGameplayAbility& AbilityCDO);
	// 만료된 버퍼 입력을 제거한다.
	void PruneExpiredCombatInputs(double NowSeconds);
	// 버퍼에서 '지금 발동 가능한 1건'을 우선순위 순으로 찾아 소비한다. 이 시스템의 유일한 발동 결정 지점.
	void ResolveBufferedCombatInput();
	// Ability.Type.Attack 자산태그를 가진 어빌리티가 현재 활성인가(= 공격 진행 중인가).
	bool IsAttackAbilityActive() const;

	TArray<FGameplayAbilitySpecHandle> InputPressedSpecHandles;
	TArray<FGameplayAbilitySpecHandle> InputReleasedSpecHandles;
	TArray<FGameplayAbilitySpecHandle> InputHeldSpecHandles;

	UPROPERTY(Transient)
	TArray<FRetrieveBufferedCombatInput> CombatInputBuffer;

	// intent별 '현재 열린 캔슬 윈도우 수'(ref-count). count>0 이면 그 intent의 캔슬이 허용된 상태.
	TMap<FGameplayTag, int32> AttackCancelIntentCounts;

	// 적재 순번 카운터(동순위 최신 우선용).
	int32 CombatInputSequence = 0;
};