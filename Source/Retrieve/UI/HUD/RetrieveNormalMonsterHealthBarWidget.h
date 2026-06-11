#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RetrieveNormalMonsterHealthBarWidget.generated.h"

class UProgressBar;

UCLASS()
class RETRIEVE_API URetrieveNormalMonsterHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI")
	void SetHealthPercent(float InPercent);

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|UI")
	FLinearColor BackgroundColor = FLinearColor(0.03f, 0.02f, 0.02f, 0.75f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|UI")
	FLinearColor FillColor = FLinearColor(0.85f, 0.05f, 0.04f, 0.95f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|UI")
	FLinearColor BorderColor = FLinearColor(0.02f, 0.02f, 0.02f, 1.f);

private:
	float HealthPercent = 1.f;
};
