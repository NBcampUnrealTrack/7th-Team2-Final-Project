#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RetrieveSettingRowToggle.generated.h"

class UButton;
class UTextBlock;
class UImage;

/**
 * On/Off 토글 설정 행.
 * - Btn_On, Btn_Off 두 버튼으로 구성. 선택된 쪽은 WBP의 프레임/배경 이미지에
 *   SelectedTint를, 비선택 쪽은 UnselectedTint를 입혀 강조한다(버튼 Style은 건드리지 않음 →
 *   WBP 디자인이 프리뷰/인게임에서 동일하게 유지된다).
 * - RowKey로 패널이 행을 식별하고 OnRowToggleChanged를 구독한다.
 *
 * WBP_SettingRow_Toggle을 이 클래스로 reparent하고 BindWidget 이름을 맞춘다:
 *   Btn_On / Btn_Off / (optional) LabelText / DescText /
 *   (optional) Img_Background_On / Img_Background_Off / Img_Frame_On / Img_Frame_Off
 */
UCLASS(Abstract, Blueprintable)
class RETRIEVE_API URetrieveSettingRowToggle : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 이 행이 제어하는 설정 키. 디자이너에서 지정. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|SettingRow")
	FName RowKey;

	/** 값 변경 시 브로드캐스트. (RowKey, bNewValue) */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRetrieveRowToggleChanged, FName, InRowKey, bool, bNewValue);
	UPROPERTY(BlueprintAssignable, Category = "Retrieve|SettingRow")
	FRetrieveRowToggleChanged OnRowToggleChanged;

	/** 값 설정 + 버튼 스타일 갱신 (브로드캐스트 없이). */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|SettingRow")
	void SetValueSilently(bool bNewValue);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|SettingRow")
	void SetLabelTexts(const FText& Label, const FText& Desc);

	UFUNCTION(BlueprintPure, Category = "Retrieve|SettingRow")
	bool GetValue() const { return bCurrentValue; }

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_On;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Off;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DescText;

	// ── 선택 강조용 이미지(WBP에 이미 존재). Refresh()가 틴트를 토글한다. ──
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Background_On;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Background_Off;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Frame_On;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Frame_Off;

	/** 선택된(활성) 쪽 프레임/배경에 곱할 틴트. 기본값(흰색)은 WBP 디자인을 그대로 노출. */
	UPROPERTY(EditAnywhere, Category = "Retrieve|SettingRow")
	FLinearColor SelectedTint = FLinearColor(1.f, 1.f, 1.f, 1.f);

	/** 비선택 쪽 프레임/배경에 곱할 틴트. 기본값은 어둡게 디밍. */
	UPROPERTY(EditAnywhere, Category = "Retrieve|SettingRow")
	FLinearColor UnselectedTint = FLinearColor(0.45f, 0.45f, 0.45f, 0.6f);

private:
	bool bCurrentValue = false;
	bool bSuppressBroadcast = false;

	void Refresh();

	UFUNCTION() void HandleOnClicked();
	UFUNCTION() void HandleOffClicked();
};
