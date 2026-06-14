#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "RetrieveNormalMonsterHealthBarWidget.generated.h"

class UProgressBar;
class UTextBlock;

UCLASS()
class RETRIEVE_API URetrieveNormalMonsterHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI")
	void SetHealthPercent(float InPercent);

	/**
	 * 몬스터 이름 · 타입 · HP 수치를 한 번에 설정.
	 * NormalMonsterHealthBarComponent.BindToHealthComponent() 시점에 호출됨.
	 * @param InName       이름 표시 텍스트
	 * @param InTypeTag    Monster.Type.Normal / Epic 등 — 이름 색상 결정에 사용
	 * @param InCurrentHP  현재 HP 수치
	 * @param InMaxHP      최대 HP 수치
	 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI")
	void SetMonsterInfo(FText InName, FGameplayTag InTypeTag, float InCurrentHP, float InMaxHP);

	/** HP 수치만 갱신 (SetHealthPercent와 함께 호출) */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI")
	void SetHPValue(float InCurrentHP, float InMaxHP);

	UFUNCTION(BlueprintImplementableEvent, Category = "Retrieve|UI")
	void PlayShowAnimation();

	UFUNCTION(BlueprintImplementableEvent, Category = "Retrieve|UI")
	void PlayHideAnimation();

protected:
	virtual void NativeOnInitialized() override;

	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HPBar;

	/** 몬스터 이름 TextBlock. WBP에서 이름을 "Text_MonsterName"으로 만들면 자동 바인딩 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_MonsterName;

	/** "현재HP / 최대HP" 숫자 TextBlock. WBP에서 "Text_HPValue"로 만들면 자동 바인딩 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_HPValue;

	/** 에픽 몬스터 전용 장식 프레임. 에픽 타입일 때만 표시 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UUserWidget> FRA_Frame;

	/** 에픽 몬스터 전용 비네트 이펙트. 에픽 타입일 때만 표시 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UUserWidget> FRA_Vignette;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|UI")
	FLinearColor BackgroundColor = FLinearColor(0.03f, 0.02f, 0.02f, 0.75f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|UI")
	FLinearColor FillColor = FLinearColor(0.85f, 0.05f, 0.04f, 0.95f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|UI")
	FLinearColor BorderColor = FLinearColor(0.02f, 0.02f, 0.02f, 1.f);

	// ── 등급별 이름 색상 ─────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|UI|MonsterType")
	FLinearColor NormalNameColor = FLinearColor(1.f, 1.f, 1.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|UI|MonsterType")
	FLinearColor EliteNameColor = FLinearColor(0.6f, 0.2f, 1.0f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|UI|MonsterType")
	FLinearColor EpicNameColor = FLinearColor(1.0f, 0.75f, 0.0f, 1.f);

private:
	void ApplyMonsterTypeColor(FGameplayTag InTypeTag);

	float HealthPercent = 1.f;
};
