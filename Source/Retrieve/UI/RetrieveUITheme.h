#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RetrieveUITheme.generated.h"

/**
 * UI 색상 테마. 기본/고대비 두 종을 만들고, 접근성 설정(High Contrast)에 따라 교체한다.
 * 위젯은 하드코딩 색상 대신 RetrieveUISettingsLibrary::GetActiveUITheme()로 색을 가져온다.
 */
UCLASS(BlueprintType)
class RETRIEVE_API URetrieveUITheme : public UDataAsset
{
	GENERATED_BODY()

public:
	// ── 텍스트 ───────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FLinearColor TextPrimary = FLinearColor(0.95f, 0.90f, 0.78f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FLinearColor TextSecondary = FLinearColor(0.70f, 0.66f, 0.55f, 1.f);

	// ── 패널/프레임 ──────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panel")
	FLinearColor Accent = FLinearColor(0.80f, 0.62f, 0.28f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panel")
	FLinearColor PanelBackground = FLinearColor(0.12f, 0.10f, 0.07f, 0.55f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panel")
	FLinearColor PanelBorder = FLinearColor(0.30f, 0.24f, 0.12f, 1.f);

	// ── 게이지/바 ────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bars")
	FLinearColor BarBackground = FLinearColor(0.08f, 0.07f, 0.05f, 0.85f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bars")
	FLinearColor HealthFill = FLinearColor(0.70f, 0.12f, 0.10f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bars")
	FLinearColor StaminaFill = FLinearColor(0.30f, 0.60f, 0.20f, 1.f);

	// ── 슬라이더 핸들(설정 화면 등) ──────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
	FLinearColor SliderHandle = FLinearColor(0.90f, 0.74f, 0.38f, 1.f);
};
