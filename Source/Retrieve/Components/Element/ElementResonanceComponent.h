#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ElementResonanceComponent.generated.h"

class UAbilitySystemComponent;
class URetrieveAbilitySystemComponent;
class URetrieveBuffUIBroadcastComponent;
class UDataTable;
struct FActiveGameplayEffect;
struct FGameplayEffectSpec;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnResonanceChanged);

/**
 * 원소 공명 판정 컴포넌트 (플레이어 전용).
 *
 * Element.Attune.Fire/Water/Wind 태그를 부여하는 스택형 GE(장비=Infinite, 흡수/아이템=Duration)의
 * 스택 수를 집계하고, DT_ElementResonance의 조건을 충족하는 행의 공명 GE를 부여/회수한다.
 * 스택 GE 추가/제거/스택수 변화 이벤트에서만 재계산하며, 결과를 diff 적용한다(성능/재진입 안전).
 *
 * 공명 GE에 UI.Buff.Resonance.* AssetTag를 달면 RetrieveBuffUIBroadcastComponent가 HUD에 자동 표기한다.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RETRIEVE_API UElementResonanceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UElementResonanceComponent();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// SovereignCharacter::InitializeAbilitySystem / UnPossessed 에서 호출
	void InitializeWithAbilitySystem(URetrieveAbilitySystemComponent* InASC);
	void UninitializeFromAbilitySystem();

	/** 현재 어튠 스택 수 (Element.Attune.Fire/Water/Wind 태그로 조회). UI용 */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Resonance")
	int32 GetElementStackCount(FGameplayTag AttuneTag) const;

	/** 현재 활성 공명 RowName 목록. UI용 */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Resonance")
	TArray<FName> GetActiveResonanceIds() const;

	/** 활성 공명이 하나라도 있으면 true. 공명 중엔 어튠 스택 적립을 막는 데 사용(GA_Absorb). */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Resonance")
	bool HasActiveResonance() const { return ActiveResonanceHandles.Num() > 0; }

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Resonance")
	FOnResonanceChanged OnResonanceChanged;

private:
	/** 공명 정의 테이블 (Row: FElementResonanceRow). 미지정 시 기본 경로에서 로드 시도 */
	UPROPERTY(EditDefaultsOnly, Category = "Resonance")
	TObjectPtr<UDataTable> ResonanceTable;

	TWeakObjectPtr<UAbilitySystemComponent> ASC;
	FDelegateHandle GEAddedHandle;
	FDelegateHandle GERemovedHandle;

	// 스택수 변화 델리게이트를 건 어튠 GE 핸들 (중복 바인딩 방지)
	TSet<FActiveGameplayEffectHandle> WatchedStackHandles;

	// 활성 공명 RowName → 적용한 공명 GE 핸들
	TMap<FName, FActiveGameplayEffectHandle> ActiveResonanceHandles;

	// GE 콜백 안에서 GE를 적용/제거하는 재진입을 피하기 위한 다음 틱 지연 플래그
	bool bRecomputePending = false;

	// 공명 변환 중 발생하는 내부 GE 추가/제거 콜백의 재계산을 막는다.
	bool bApplyingResonanceTransaction = false;

	bool HasAuthorityToModify() const;
	void ResolveResonanceTable();

	void OnGameplayEffectAdded(UAbilitySystemComponent* InASC, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle);
	void OnGameplayEffectRemoved(const FActiveGameplayEffect& RemovedGE);
	void OnAttuneStackChanged(FActiveGameplayEffectHandle Handle, int32 NewStackCount, int32 PreviousStackCount);

	void ScheduleRecompute();
	void RecomputeResonance();

	int32 CountAttuneStacks(const FGameplayTag& AttuneTag) const;

	/** 유한 지속시간(흡수/아이템) 어튠 스택만 집계 — 장비(Infinite)는 제외. 소모/발동 판정용. */
	int32 CountAbsorbStacks(const FGameplayTag& AttuneTag) const;

	/** 공명 재료로 사용한 흡수 스택과 대응하는 능력치 버프 GE를 지정 수만큼 제거. */
	void ConsumeAbsorbStacks(const FGameplayTag& AttuneTag, int32 StacksToConsume);

	static bool IsAttuneSpec(const FGameplayEffectSpec& Spec);
};
