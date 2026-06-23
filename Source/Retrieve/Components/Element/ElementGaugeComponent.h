#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "ElementGaugeComponent.generated.h"

class URetrieveAbilitySystemComponent;
class UDataTable;
struct FGameplayEventData;

USTRUCT(BlueprintType)
struct FElementSlot
{
	GENERATED_BODY()

	// 슬롯 현재 게이지
	UPROPERTY(BlueprintReadOnly, Category = "Element")
	int32 InternalGauge = 0;
	// 슬롯 최대 게이지
	UPROPERTY(BlueprintReadOnly, Category = "Element")
	int32 MaxGauge = 100;
	UPROPERTY(BlueprintReadOnly, Category = "Element")
	bool bFull = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnElementSlotsChanged);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class RETRIEVE_API UElementGaugeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UElementGaugeComponent();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// 게이지 추가
	UFUNCTION(BlueprintCallable, Category = "Gauge")
	void AddCharge(int32 Amount);
	// 슬롯 확정
	UFUNCTION(BlueprintCallable, Category = "Gauge")
	void CommitSlot();
	// 슬롯이 가득 차있는지 확인
	UFUNCTION(BlueprintPure, Category = "Gauge")
	bool IsFull() const;
	// 첫 번째(가장 오래된) 충전 슬롯을 1칸 소비. 소비했으면 true. 원소는 더 이상 구분하지 않는다.
	UFUNCTION(BlueprintCallable, Category = "Gauge")
	bool ConsumeOldestSlot();
	// 슬롯 초기화
	UFUNCTION(BlueprintCallable, Category = "Gauge")
	void ClearSlot();

	URetrieveAbilitySystemComponent* GetRetrieveASC() const;

	// ASC GameplayEvent 구독/해제. SovereignCharacter::InitializeAbilitySystem 에서 호출
	void BindToASC();
	void UnbindFromASC();

	// 아이템 사용 시 호출. Duration 초 동안 충전 배율에 Multiplier를 곱한다. Duration=0이면 즉시 해제.
	// ElementTag는 만료 시 Channel.UI.Buff.Remove 브로드캐스트에 사용된다.
	void SetItemChargeMultiplier(float Multiplier, float Duration, FGameplayTag BuffUITag = FGameplayTag());

	// 아이템 버프로 적용된 현재 배율 (UI 표시용)
	UFUNCTION(BlueprintPure, Category = "Gauge|UI")
	float GetItemChargeMultiplier() const { return ItemChargeMultiplier; }

	// 슬롯 상태 변경 시 브로드캐스트 (ElementGaugeViewModel 구독용)
	UPROPERTY(BlueprintAssignable)
	FOnElementSlotsChanged OnSlotsChanged;

	// UI용: 슬롯 배열 읽기
	UFUNCTION(BlueprintPure, Category = "Gauge|UI")
	const TArray<FElementSlot>& GetElementSlots() const { return ElementSlots; }

	// UI용: 슬롯 게이지 비율(0~1)
	UFUNCTION(BlueprintPure, Category = "Gauge|UI")
	float GetSlotRatio(int32 SlotIndex) const;

	// UI용: 슬롯 개수(최대치). 주의: 현재 채워진 수가 아니라 슬롯 칸 수(3)를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Gauge|UI")
	int32 GetSlotCount() const { return SlotCount; }

	// 소비 가능한(충전 완료된) 슬롯이 하나라도 있는지 검사, 잔량 확인엔 GetSlotCount()가 아니라 이 함수 사용
	UFUNCTION(BlueprintPure, Category = "Gauge|UI")
	bool HasChargedSlot() const { return ElementSlots.Num() > 0 && ElementSlots[0].bFull; }

private:
	void HandleGameplayEvent(FGameplayTag EventTag, const FGameplayEventData* Payload);

	// AddCharge 에서 배율 적용 후 실제 누적 처리. 재귀 시 배율 중복 적용을 막기 위해 분리.
	void AddChargeInternal(int32 ScaledAmount);
	void BroadcastGaugeFull() const;

	// 이벤트→충전량 매핑 데이터 테이블 (Row 타입: FElementChargeRule)
	UPROPERTY(EditDefaultsOnly, Category="Gauge|Charge")
	TObjectPtr<UDataTable> ChargeRuleTable;

	// 런타임 캐시. BindToASC 호출 시 1회 빌드
	TMap<FGameplayTag, int32> ChargeRuleCache;

	// 구독 필터 (캐시 키들의 컨테이너) - 해제 시 동일 필터 필요
	FGameplayTagContainer SubscribedFilter;

	FDelegateHandle GameplayEventHandle;

	// 아이템 버프 배율. SetItemChargeMultiplier로 설정하고 타이머 만료 시 1.0으로 복원.
	float ItemChargeMultiplier = 1.f;
	FTimerHandle ItemMultiplierTimer;

	TArray<FElementSlot> ElementSlots;
	const int32 SlotCount = 3;
	int32 CurrentSlotIndex;

	UPROPERTY(VisibleAnywhere)
	mutable TWeakObjectPtr<URetrieveAbilitySystemComponent> ASC;
};
