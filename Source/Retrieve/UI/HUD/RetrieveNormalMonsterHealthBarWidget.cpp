#include "UI/HUD/RetrieveNormalMonsterHealthBarWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "GameplayTagsManager.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace RetrieveMonsterHPBar
{
	// FMonsterDataRow.MonsterType 태그 계층
	const FName TagElite(TEXT("Monster.Type.Elite"));
	const FName TagEpic(TEXT("Monster.Type.Epic"));
}

void URetrieveNormalMonsterHealthBarWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// BindWidgetOptional 실패 시 이름으로 직접 검색.
	// WidgetTree->RootWidget을 교체하지 않음 — Canvas Panel(FRA_Frame 등)을 파괴하는 것을 방지.
	if (!HPBar && WidgetTree)
	{
		HPBar = WidgetTree->FindWidget<UProgressBar>(TEXT("HPBar"));
	}
	if (!FRA_Frame && WidgetTree)
	{
		FRA_Frame = Cast<UUserWidget>(WidgetTree->FindWidget(TEXT("FRA_Frame")));
	}
	if (!FRA_Vignette && WidgetTree)
	{
		FRA_Vignette = Cast<UUserWidget>(WidgetTree->FindWidget(TEXT("FRA_Vignette")));
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[MonsterHPBar] NativeOnInitialized: HPBar=%s, FRA_Frame=%s, FRA_Vignette=%s, WidgetClass=%s"),
		HPBar ? TEXT("OK") : TEXT("NULL"),
		FRA_Frame ? TEXT("OK") : TEXT("NULL"),
		FRA_Vignette ? TEXT("OK") : TEXT("NULL"),
		GetClass() ? *GetClass()->GetName() : TEXT("NULL"));

	if (HPBar)
	{
		HPBar->SetPercent(HealthPercent);
		HPBar->SetFillColorAndOpacity(FillColor);
	}

	// 에픽 전용 프레임은 기본적으로 숨김 — ApplyMonsterTypeColor에서 에픽일 때만 표시
	if (FRA_Frame)    FRA_Frame->SetVisibility(ESlateVisibility::Collapsed);
	if (FRA_Vignette) FRA_Vignette->SetVisibility(ESlateVisibility::Collapsed);
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

void URetrieveNormalMonsterHealthBarWidget::SetMonsterInfo(
	FText InName, FGameplayTag InTypeTag, float InCurrentHP, float InMaxHP)
{
	if (Text_MonsterName)
	{
		Text_MonsterName->SetText(InName);
	}
	// Text_MonsterName 바인딩 여부와 무관하게 항상 호출 — 프레임 visibility도 여기서 처리됨
	ApplyMonsterTypeColor(InTypeTag);

	SetHPValue(InCurrentHP, InMaxHP);

	const float Percent = InMaxHP > 0.f
		? FMath::Clamp(InCurrentHP / InMaxHP, 0.f, 1.f) : 0.f;
	SetHealthPercent(Percent);
}

void URetrieveNormalMonsterHealthBarWidget::SetHPValue(float InCurrentHP, float InMaxHP)
{
	if (!Text_HPValue)
	{
		return;
	}

	const int32 CurHP = FMath::CeilToInt(FMath::Max(0.f, InCurrentHP));
	const int32 MaxHP = FMath::CeilToInt(FMath::Max(0.f, InMaxHP));
	Text_HPValue->SetText(FText::FromString(
		FString::Printf(TEXT("%d / %d"), CurHP, MaxHP)));
}

void URetrieveNormalMonsterHealthBarWidget::ApplyMonsterTypeColor(FGameplayTag InTypeTag)
{
	bool bIsEpic = false;
	FLinearColor NameColor = NormalNameColor;

	if (InTypeTag.IsValid())
	{
		const FName TagName = InTypeTag.GetTagName();
		if (TagName == RetrieveMonsterHPBar::TagEpic)
		{
			NameColor = EpicNameColor;
			bIsEpic = true;
		}
		else if (TagName == RetrieveMonsterHPBar::TagElite)
		{
			NameColor = EliteNameColor;
		}
	}

	if (Text_MonsterName)
	{
		Text_MonsterName->SetColorAndOpacity(FSlateColor(NameColor));
	}

	// 에픽일 때만 프레임 표시 — Text_MonsterName 바인딩 여부와 무관하게 항상 실행
	const ESlateVisibility FrameVis = bIsEpic
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed;

	UE_LOG(LogTemp, Warning,
		TEXT("[MonsterHPBar] ApplyMonsterTypeColor: Tag=%s, bIsEpic=%d, FRA_Frame=%s, FRA_Vignette=%s"),
		*InTypeTag.ToString(), bIsEpic ? 1 : 0,
		FRA_Frame ? TEXT("OK") : TEXT("NULL"),
		FRA_Vignette ? TEXT("OK") : TEXT("NULL"));

	if (FRA_Frame)    FRA_Frame->SetVisibility(FrameVis);
	if (FRA_Vignette) FRA_Vignette->SetVisibility(FrameVis);
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
