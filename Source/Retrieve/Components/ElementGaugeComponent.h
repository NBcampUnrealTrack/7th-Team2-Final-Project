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
	bool bIsFull = false;
	// 슬롯 원소
	UPROPERTY(BlueprintReadOnly, Category = "Element")
	FGameplayTag CurrentElement = RetrieveGameplayTags::Element_None;
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class RETRIEVE_API UElementGaugeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UElementGaugeComponent();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// 게이지 추가
	void AddCharge(int32 Amount);
	// 슬롯 확정
	void CommitSlot();
	// 슬롯이 가득 차있는지 확인
	bool IsFull();
	// 현재 조합 반환
	TMap<FGameplayTag, int32> GetCurrentCombination();
	// 첫 번째 슬롯 소비
	FGameplayTag ConsumeOldestSlot();
	// 전체 슬롯 소비
	void ConsumeAllSlots();
	// 슬롯 초기화
	void ClearSlot();

	URetrieveAbilitySystemComponent* GetRetrieveASC() const;

	// ASC GameplayEvent 구독/해제. SovereignCharacter::InitializeAbilitySystem 에서 호출
	void BindToASC();
	void UnbindFromASC();

	// UI용: 슬롯 배열 읽기
	UFUNCTION(BlueprintPure, Category = "Gauge|UI")
	const TArray<FElementSlot>& GetElementSlots() const { return ElementSlots; }

	// UI용: 슬롯 게이지 비율(0~1)
	UFUNCTION(BlueprintPure, Category = "Gauge|UI")
	float GetSlotRatio(int32 SlotIndex) const;

	// UI용: 슬롯 개수
	UFUNCTION(BlueprintPure, Category = "Gauge|UI")
	int32 GetSlotCount() const { return SlotCount; }

private:
	void HandleGameplayEvent(FGameplayTag EventTag, const FGameplayEventData* Payload);

	// 이벤트→충전량 매핑 데이터 테이블 (Row 타입: FElementChargeRule)
	UPROPERTY(EditDefaultsOnly, Category="Gauge|Charge")
	TObjectPtr<UDataTable> ChargeRuleTable;

	// 런타임 캐시. BindToASC 호출 시 1회 빌드
	TMap<FGameplayTag, int32> ChargeRuleCache;

	// 구독 필터 (캐시 키들의 컨테이너) - 해제 시 동일 필터 필요
	FGameplayTagContainer SubscribedFilter;

	FDelegateHandle GameplayEventHandle;

	TArray<FElementSlot> ElementSlots;
	const int32 SlotCount = 3;
	int32 CurrentSlotIndex;

	UPROPERTY(VisibleAnywhere)
	mutable TWeakObjectPtr<URetrieveAbilitySystemComponent> ASC;
};
