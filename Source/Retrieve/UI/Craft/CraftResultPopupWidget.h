#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CraftResultPopupWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UTexture2D;

/**
 * 강화 성공/실패 결과 팝업. WBP_CraftResultPopup의 C++ 부모 클래스.
 *
 * WBP 구성:
 *   Image_ResultIcon  — 결과 아이콘
 *   Text_ResultTitle  — "강화 성공!" / "강화 실패..."
 *   Button_Confirm    — 닫기
 */
UCLASS()
class RETRIEVE_API UCraftResultPopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 성공/실패에 맞는 아이콘·문구·색상으로 팝업을 채우고 표시한다. */
	void ShowResult(bool bSuccess, UTexture2D* Icon);

	/** 배치(여러 개 확률 제작) 결과를 "성공 N / 실패 M" 요약으로 채우고 1회만 표시한다. */
	void ShowResultSummary(int32 SuccessCount, int32 FailCount, UTexture2D* Icon);

	void HideResult();

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_ResultIcon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ResultTitle;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Confirm;

private:
	UFUNCTION()
	void HandleConfirmClicked();

	static const FLinearColor SuccessColor;
	static const FLinearColor FailureColor;
};
