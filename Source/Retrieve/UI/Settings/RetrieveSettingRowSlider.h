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

	/**
	 * 이 행이 나타내는 실제 값의 최소/최대. 내부 USlider는 항상 0..1이며,
	 * OnRowValueChanged와 ValueText 표시는 이 범위로 매핑된 실값(Raw) 기준이다.
	 * 기본값(0..1)을 쓰면 볼륨처럼 0..1 자체가 실값인 행은 그대로 동작한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|SettingRow")
	float RangeMin = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|SettingRow")
	float RangeMax = 1.f;

	/** true면 ValueText를 0..1 정규화값 기준 %로 표시(기존 방식, 볼륨류). false면 Raw 값을 DisplayDecimals 자리로 표시. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|SettingRow")
	bool bDisplayAsPercent = true;

	/** bDisplayAsPercent가 false일 때 ValueText에 표시할 소수 자릿수. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|SettingRow")
	int32 DisplayDecimals = 0;

	/** 값(Range로 매핑된 실값) 변경 시 브로드캐스트. (RowKey, NewValue) */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRetrieveRowValueChanged, FName, InRowKey, float, NewValue);
	UPROPERTY(BlueprintAssignable, Category = "Retrieve|SettingRow")
	FRetrieveRowValueChanged OnRowValueChanged;

	/** 슬라이더 정규화값(0..1) 설정 + 표시 갱신(브로드캐스트 없이). Range가 기본(0..1)인 행에서 사용. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|SettingRow")
	void SetValueSilently(float Value01);

	/** 실제 단위 값(Range로 매핑됨) 설정 + 표시 갱신(브로드캐스트 없이). Range를 지정한 행에서 사용. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|SettingRow")
	void SetRawValueSilently(float RawValue);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|SettingRow")
	void SetLabelTexts(const FText& Label, const FText& Desc);

	UFUNCTION(BlueprintPure, Category = "Retrieve|SettingRow")
	float GetValue01() const;

	/** 현재 슬라이더 값을 Range로 매핑한 실값. */
	UFUNCTION(BlueprintPure, Category = "Retrieve|SettingRow")
	float GetRawValue() const;

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
