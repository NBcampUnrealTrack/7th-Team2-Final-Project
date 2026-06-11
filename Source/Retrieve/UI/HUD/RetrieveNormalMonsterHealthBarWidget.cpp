#include "UI/HUD/RetrieveNormalMonsterHealthBarWidget.h"

#include "Components/ProgressBar.h"
#include "Blueprint/WidgetTree.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

void URetrieveNormalMonsterHealthBarWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!HPBar && WidgetTree)
	{
		HPBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HPBar"));
		WidgetTree->RootWidget = HPBar;
	}

	if (HPBar)
	{
		HPBar->SetPercent(HealthPercent);
		HPBar->SetFillColorAndOpacity(FillColor);
	}
}

void URetrieveNormalMonsterHealthBarWidget::SetHealthPercent(float InPercent)
{
	HealthPercent = FMath::Clamp(InPercent, 0.f, 1.f);

	if (HPBar)
	{
		HPBar->SetPercent(HealthPercent);
	}

	Invalidate(EInvalidateWidgetReason::Paint);
}

int32 URetrieveNormalMonsterHealthBarWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	const int32 FinalLayer = Super::NativePaint(
		Args,
		AllottedGeometry,
		MyCullingRect,
		OutDrawElements,
		LayerId,
		InWidgetStyle,
		bParentEnabled);

	if (HPBar)
	{
		return FinalLayer;
	}

	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	if (LocalSize.X <= 0.f || LocalSize.Y <= 0.f)
	{
		return FinalLayer;
	}

	constexpr float BorderThickness = 1.f;
	const FVector2D BackgroundSize(
		FMath::Max(0.f, LocalSize.X - BorderThickness * 2.f),
		FMath::Max(0.f, LocalSize.Y - BorderThickness * 2.f));
	const FVector2D FillSize(
		BackgroundSize.X * HealthPercent,
		BackgroundSize.Y);

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		FinalLayer + 1,
		AllottedGeometry.ToPaintGeometry(),
		FCoreStyle::Get().GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		BorderColor);

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		FinalLayer + 2,
		AllottedGeometry.ToPaintGeometry(
			FVector2D(BorderThickness, BorderThickness),
			BackgroundSize),
		FCoreStyle::Get().GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		BackgroundColor);

	if (FillSize.X > 0.f && FillSize.Y > 0.f)
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			FinalLayer + 3,
			AllottedGeometry.ToPaintGeometry(FVector2D(BorderThickness, BorderThickness), FillSize),
			FCoreStyle::Get().GetBrush("WhiteBrush"),
			ESlateDrawEffect::None,
			FillColor);
	}

	return FinalLayer + 3;
}
