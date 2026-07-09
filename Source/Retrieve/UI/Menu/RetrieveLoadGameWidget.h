#pragma once

#include "CoreMinimal.h"
#include "UI/RetrieveGamePanelWidget.h"
#include "RetrieveLoadGameWidget.generated.h"

class UScrollBox;
class UImage;
class UOverlay;
class UTextBlock;
class UUserWidget;
class UTexture2D;
class ARetrievePlayerController;

/**
 * WBP_LoadGame의 C++ 베이스 클래스.
 * 메인메뉴 "불러오기" 버튼 -> PlayerController::OpenLoadGamePanel()로 연다.
 *
 * ScrollBox_LoadSlots에 디자이너에서 미리 배치한 WBP_SaveSlotEntry(모닥불 저장탭과 동일 위젯)
 * 5개(SlotIndex 0~4)를 저장 데이터로 채우고, 각 엔트리 클릭 시 해당 슬롯을 즉시 불러온다.
 * WBP_SaveSlotEntry의 OwnerMenu는 WBP_BonfireMenu_C 전용으로 타입이 고정돼 있어 그대로 재사용할 수
 * 없으므로, 여기서는 엔트리의 Button을 C++에서 직접 바인딩한다(WBP_SaveSlotEntry 수정 없음).
 */
UCLASS(Abstract, Blueprintable)
class RETRIEVE_API URetrieveLoadGameWidget : public URetrieveGamePanelWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> ScrollBox_LoadSlots;

	// ── 우측 프리뷰 패널 (선택/호버된 슬롯 정보) ──
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_PreviewThumbnail;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_PreviewBonfireName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_PreviewTimestamp;

	/** 저장 시점 추적 퀘스트 표시. WBP에 없으면 Text_PreviewTimestamp 옆에 동적으로 생성한다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_PreviewQuestName;

private:
	void RefreshSlotEntries();
	void ApplyThumbnailToEntry(UUserWidget* EntryWidget, int32 SlotIndex);
	void BindEntryInteractions(UUserWidget* EntryWidget, int32 SlotIndex);
	void UpdatePreview(int32 SlotIndex);
	void RequestLoadSlot(int32 SlotIndex);

	/** 클릭한 엔트리에 골드 글로우(선택 하이라이트)를 적용하고 나머지는 해제한다. */
	void HighlightClickedEntry(int32 SlotIndex);

	UFUNCTION() void HandleSlot0Clicked();
	UFUNCTION() void HandleSlot1Clicked();
	UFUNCTION() void HandleSlot2Clicked();
	UFUNCTION() void HandleSlot3Clicked();
	UFUNCTION() void HandleSlot4Clicked();

	UFUNCTION() void HandleSlot0Hovered();
	UFUNCTION() void HandleSlot1Hovered();
	UFUNCTION() void HandleSlot2Hovered();
	UFUNCTION() void HandleSlot3Hovered();
	UFUNCTION() void HandleSlot4Hovered();

	UFUNCTION() void HandleCloseButtonClicked();

	ARetrievePlayerController* GetRetrievePlayerController() const;

	/** PNG에서 만든 런타임 리스트 썸네일 텍스처의 GC 방지 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> SlotThumbnailTextures;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> PreviewThumbnailTexture;
};
