#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "ElementGaugeComponent.generated.h"

class URetrieveAbilitySystemComponent;

struct FElementSlot
{
	// 슬롯 현재 게이지
	int32 InternalGauge = 0;
	// 슬롯 최대 게이지
	int32 MaxGauge = 100;
	bool bIsFull = false;
	// 슬롯 원소
	FGameplayTag CurrentElement = RetrieveGameplayTags::Element_None;
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class RETRIEVE_API UElementGaugeComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UElementGaugeComponent();

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

private:
	TArray<FElementSlot> ElementSlots;
	const int32 SlotCount = 3;
	int32 CurrentSlotIndex;

	UPROPERTY(VisibleAnywhere)
	mutable TWeakObjectPtr<URetrieveAbilitySystemComponent> ASC;
};
