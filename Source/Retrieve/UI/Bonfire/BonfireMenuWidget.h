#pragma once

#include "CoreMinimal.h"
#include "UI/RetrieveGamePanelWidget.h"
#include "BonfireMenuWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UWidget;
class UTexture2D;
class UUserWidget;
class URetrieveSaveSubsystem;
class URetrieveTimedActionWidget;

/**
 * 모닥불 메뉴 위젯의 C++ 기반 클래스.
 * WBP_BonfireMenu의 부모 클래스로 지정한다.
 * PlayerController의 OpenExclusivePanel 흐름을 통해 열리므로
 * 커서·입력 모드 관리는 PlayerController에 완전히 위임된다.
 */
UCLASS(Abstract, Blueprintable)
class RETRIEVE_API UBonfireMenuWidget : public URetrieveGamePanelWidget
{
	GENERATED_BODY()

public:
	/** BonfireActor가 패널 생성 직후 주입하는 화톳불 식별자 */
	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Bonfire")
	FName BonfireId;

	/** 활성 탭 시각 갱신 (true=저장 탭 활성). BP 탭 클릭에서도 호출 가능 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Bonfire")
	void SetActiveTab(bool bSaveActive);

	/** WBP에 내장된 TimedActionWidget(Panel_ConfirmOverwrite와 동일한 패턴)을 표시하고 재생한다. */
	void ShowTimedAction(float Duration, const FText& ActionText, FSimpleDelegate OnComplete);

	/** TimedActionWidget을 숨긴다. */
	void HideTimedAction();

	/** WBP에 내장된 CraftResultPopupWidget에 강화 성공/실패 결과를 표시한다. */
	void ShowCraftResult(bool bSuccess, UTexture2D* Icon);

	/** 배치(여러 개 확률 제작) 결과를 "성공 N / 실패 M" 요약 팝업으로 1회 표시한다. */
	void ShowCraftResultSummary(int32 SuccessCount, int32 FailCount, UTexture2D* Icon);

	// ── 탭 색상 (에디터에서 조정 가능, 재빌드 불필요) ──
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Bonfire|Tab")
	FLinearColor TabActiveColor = FLinearColor(0.10f, 0.42f, 0.82f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Bonfire|Tab")
	FLinearColor TabInactiveColor = FLinearColor(0.025f, 0.07f, 0.11f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Bonfire|Tab")
	FLinearColor TabActiveTextColor = FLinearColor(1.0f, 0.95f, 0.80f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Bonfire|Tab")
	FLinearColor TabInactiveTextColor = FLinearColor(0.55f, 0.60f, 0.68f, 1.0f);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_TabSave;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_TabCraft;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_TabSave;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_TabCraft;

	// 초기 활성 탭 자동 감지용 (보이는 패널 기준)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Panel_Craft;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Save;

private:
	void ApplyFantasyMenuStyle();
	void UpdateStyledButtonBackground(UButton* Button, UImage* Background, bool bSelected) const;
	void PerformSelectedSlotSave();

	UFUNCTION()
	void HandleSaveButtonClicked();

	UFUNCTION()
	void HandleConfirmOverwriteClicked();

	UFUNCTION()
	void HandleCancelOverwriteClicked();

	UFUNCTION()
	void HandleSaveCompleted();

	UFUNCTION()
	void RefreshSlotThumbnails();

	void QueueThumbnailRefresh(int32 AttemptCount);
	void ApplyThumbnailToEntry(UUserWidget* EntryWidget, int32 FallbackSlotIndex);
	void UpdateSlotSelectionVisuals();
	static int32 ResolveSlotIndex(const UUserWidget* EntryWidget, int32 FallbackSlotIndex);
	void RefreshSelectedSlotPreview(UUserWidget* SelectedEntry, int32 SelectedSlotIndex);
	UTexture2D* GetOrDecodeSlotScreenshot(int32 SlotIndex);

	UFUNCTION()
	void HandleTabSaveClicked();

	UFUNCTION()
	void HandleTabCraftClicked();

	/** PNG에서 만든 런타임 텍스처의 GC 방지. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> SlotThumbnailTextures;
	
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UTexture2D>> SlotPreviewTextures;

	UPROPERTY(Transient)
	TObjectPtr<UButton> RuntimeButtonLoad;

	UPROPERTY(Transient)
	TObjectPtr<UImage> ButtonBackground_TabSave;

	UPROPERTY(Transient)
	TObjectPtr<UImage> ButtonBackground_TabCraft;

	UPROPERTY(Transient)
	TObjectPtr<UImage> ButtonBackground_Save;

	UPROPERTY(Transient)
	TObjectPtr<UImage> ButtonBackground_Load;

	bool bSaveTabActive = true;
	int32 ThumbnailRefreshAttemptsRemaining = 0;
	int32 LastAppliedSelectedSlotIndex = INDEX_NONE;
	int32 LastAppliedSlotEntryCount = INDEX_NONE;
};
