#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "RetrieveBuffBarWidget.generated.h"

class UPanelWidget;
class URetrieveBuffSlotWidget;

/**
 * 버프/디버프 바 컨테이너.
 * Channel_UI_Buff_Apply/Remove 를 수신해 슬롯 풀을 관리한다.
 * BuffId가 같으면 기존 슬롯을 갱신하고, 새 것이면 빈 슬롯에 배정한다.
 * WBP_BuffBar의 부모 클래스로 설정한다.
 */
UCLASS()
class RETRIEVE_API URetrieveBuffBarWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	/** WBP 안에 이 이름의 패널 위젯이 있어야 한다 (HorizontalBox 또는 WrapBox) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UPanelWidget> HB_Slots;

	/** BP Details에서 WBP_BuffSlot 에셋을 지정한다 */
	UPROPERTY(EditDefaultsOnly, Category = "BuffBar")
	TSubclassOf<URetrieveBuffSlotWidget> BuffSlotClass;

	/** 동시에 표시할 수 있는 최대 버프 수 */
	UPROPERTY(EditDefaultsOnly, Category = "BuffBar", meta = (ClampMin = 1, ClampMax = 16))
	int32 MaxSlots = 8;

	UPROPERTY()
	TArray<TObjectPtr<URetrieveBuffSlotWidget>> SlotPool;

	TArray<float> SlotRemaining;
	TMap<FGameplayTag, int32> ActiveBuffToSlot;

	FGameplayMessageListenerHandle ApplyHandle;
	FGameplayMessageListenerHandle RemoveHandle;
	FTSTicker::FDelegateHandle TickHandle;

	void InitSlotPool();
	void OnBuffApply(FGameplayTag Channel, const FRetrieveUIBuffPayload& Payload);
	void OnBuffRemove(FGameplayTag Channel, const FRetrieveUIBuffRemovePayload& Payload);
	bool TickAllSlots(float DeltaTime);
	int32 FindFreeSlot() const;
	void ClearSlot(int32 SlotIndex);
};
