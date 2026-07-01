#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RetrieveSettingRowSlider.generated.h"

class USlider;
class UButton;
class UTextBlock;
class UWidget;

/**
 * 재사용 슬라이더 설정 행(image 2 스타일).
 * - 트랙 위를 드래그하거나(투명 USlider) 양옆 화살표 버튼으로 값을 조정한다.
 * - 값 박스("100")가 슬라이더의 썸처럼 트랙을 따라 이동하며 숫자가 갱신된다.
 * - 패널은 이 행들을 RowKey로 식별해 값 변경을 구독한다.
 *
 * WBP_SettingRow_Slider를 이 클래스로 reparent하고, 내부에 BindWidget 이름으로
 *   Slider(USlider, 투명/드래그용), ValueText(UTextBlock), ValBox(값 박스 위젯, CanvasPanel 슬롯)를 둔다.
 *   선택: ArrowLeft/ArrowRight(UButton), LabelText/DescText(UTextBlock).
 */
UCLASS(Abstract, Blueprintable)
class RETRIEVE_API URetrieveSettingRowSlider : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 이 행이 제어하는 설정 키(예: "Master","Music","Sfx","UI","Ambience"). 디자이너에서 지정. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|SettingRow")
	FName RowKey;

	/** 화살표 1회 클릭 시 증감량(0..1 정규화). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|SettingRow")
	float StepAmount = 0.05f;

	/** 트랙 너비(값 박스 이동 범위 계산용, px). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|SettingRow")
	float TrackWidth = 320.f;

	/** 값 박스 너비(이동 시 트랙 밖으로 나가지 않도록, px). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|SettingRow")
	float ValueBoxWidth = 64.f;

	/** 값(0..1) 변경 시 브로드캐스트. (RowKey, NewValue) */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRetrieveRowValueChanged, FName, InRowKey, float, NewValue);
	UPROPERTY(BlueprintAssignable, Category = "Retrieve|SettingRow")
	FRetrieveRowValueChanged OnRowValueChanged;

	/** 값(0..1) 설정 + 표시 갱신(브로드캐스트 없이). */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|SettingRow")
	void SetValueSilently(float Value01);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|SettingRow")
	void SetLabelTexts(const FText& Label, const FText& Desc);

	UFUNCTION(BlueprintPure, Category = "Retrieve|SettingRow")
	float GetValue01() const;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USlider> Slider;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ValueText;

	/** 트랙을 따라 이동하는 값 박스(썸 역할). CanvasPanel의 자식이어야 위치 이동이 적용된다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> ValBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DescText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ArrowLeft;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ArrowRight;

private:
	bool bSuppressBroadcast = false;

	/** 값 텍스트 갱신 + 값 박스를 트랙상의 값 위치로 이동. */
	void Refresh(float Value01);

	UFUNCTION() void HandleSliderChanged(float Value);
	UFUNCTION() void HandleArrowLeft();
	UFUNCTION() void HandleArrowRight();
};
