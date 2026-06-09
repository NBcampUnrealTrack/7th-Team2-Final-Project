#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "RetrieveElementGaugeWidget.generated.h"

class UImage;
class UProgressBar;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UElementGaugeViewModel;

/**
 * WBP_ElementGauge의 부모 C++ 클래스.
 *
 * Image + Material Instance Dynamic 방식으로 게이지를 렌더링한다.
 * - 원소별 MI(MI_ElementGauge_Fire 등)를 BP Details에서 지정
 * - 슬롯 원소가 바뀌면 해당 MI로 DMI를 교체
 * - 충전량이 바뀌면 Percent 보간 애니메이션
 * - 충전 증가/슬롯 확정 시 GlowPower 펄스 애니메이션
 * - Image_Fill_* 없을 때 ProgressBar_Slot* 폴백 동작
 *
 * 머티리얼 파라미터 규격 (M_ElementGauge_Fill):
 *   Scalar Parameter  : Percent        (0~1, 게이지 채움량. C++가 보간 후 설정)
 *   Scalar Parameter  : Softness       (가장자리 부드러움, 기본 0.02)
 *   Scalar Parameter  : GlowPower      (글로우 강도 0~4, 충전/확정 시 C++가 애니메이션)
 *   Vector Parameter  : FillColor      (채움 색상. MI 인스턴스에서 원소별로 지정)
 *   Vector Parameter  : EmptyColor     (비어있는 영역 색상)
 */
UCLASS()
class RETRIEVE_API URetrieveElementGaugeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ─── Image 위젯 (WBP에서 이름이 정확히 일치해야 함) ──────────────────────
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Fill_0;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Fill_1;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Fill_2;

	// ─── ProgressBar 폴백 위젯 (Image_Fill_* 없을 때 사용) ──────────────────
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ProgressBar_Slot0;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ProgressBar_Slot1;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ProgressBar_Slot2;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Element;

	// ─── 원소별 머티리얼 인스턴스 (BP Details에서 에셋 지정) ─────────────────
	UPROPERTY(EditDefaultsOnly, Category = "ElementGauge|Materials")
	TObjectPtr<UMaterialInterface> MI_Fire;

	UPROPERTY(EditDefaultsOnly, Category = "ElementGauge|Materials")
	TObjectPtr<UMaterialInterface> MI_Water;

	UPROPERTY(EditDefaultsOnly, Category = "ElementGauge|Materials")
	TObjectPtr<UMaterialInterface> MI_Wind;

	/** 원소 없음(비어있는 슬롯)에 사용할 기본 MI */
	UPROPERTY(EditDefaultsOnly, Category = "ElementGauge|Materials")
	TObjectPtr<UMaterialInterface> MI_Empty;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	static constexpr int32 SlotCount = 3;

	// ─── DMI 상태 ───────────────────────────────────────────────────────────
	TObjectPtr<UMaterialInstanceDynamic> SlotDMIs[SlotCount];
	FGameplayTag CachedElements[SlotCount];

	// ─── Percent 보간 상태 ───────────────────────────────────────────────────
	float TargetRatios[SlotCount]  = {};
	float CurrentRatios[SlotCount] = {};

	// ─── GlowPower 펄스 상태 ─────────────────────────────────────────────────
	bool  bGlowActive[SlotCount]  = {};
	float GlowProgress[SlotCount] = {};  ///< 0에서 GlowDuration까지 증가
	float GlowDuration[SlotCount] = {};  ///< 펄스 총 지속 시간
	float GlowPeaks[SlotCount]    = {};  ///< sin 곡선 최대 강도

	// ─── 슬롯 확정 전환 감지 ─────────────────────────────────────────────────
	bool bPrevFull[SlotCount] = {};

	FTSTicker::FDelegateHandle TickHandle;

	TWeakObjectPtr<UElementGaugeViewModel> BoundViewModel;
	FGameplayTag CachedCurrentElement;

	bool bElementModePulseActive = false;
	float ElementModePulseProgress = 0.f;
	float ElementModePulseDuration = 0.35f;
	FLinearColor ElementModePulseColor = FLinearColor::White;

	void InitFromViewModel(UElementGaugeViewModel* GaugeVM);

	UFUNCTION()
	void HandleGaugeUpdated();

	UFUNCTION()
	void HandleCurrentElementChanged(FGameplayTag NewElement);

	/**
	 * 슬롯 비율/원소 갱신.
	 * @param bFull       슬롯이 확정 완료됐는지
	 * @param bImmediate  true이면 보간 없이 즉시 반영 (초기화 시)
	 */
	void UpdateSlot(int32 SlotIndex, float Ratio, FGameplayTag Element, bool bFull, bool bImmediate = false);

	/** 매 프레임: Percent 보간 + GlowPower sin 펄스 */
	bool TickAnimation(float DeltaTime);

	/** GlowPower 펄스 트리거. bIsFullTransition이면 더 강하고 길다 */
	void TriggerGlowPulse(int32 SlotIndex, bool bIsFullTransition);
	void TriggerElementModePulse(FGameplayTag NewElement, bool bImmediate);
	void DispatchElementModeChangedToBlueprint(FGameplayTag NewElement);
	void SetElementIconVisualState(float PulseAlpha);

	UImage*              GetSlotImage(int32 SlotIndex) const;
	UProgressBar*        GetSlotProgressBar(int32 SlotIndex) const;
	UMaterialInterface*  GetMIForElement(const FGameplayTag& Element) const;
};
