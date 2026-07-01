#include "UI/RetrieveThemedBarWidget.h"
#include "UI/RetrieveUISettingsLibrary.h"
#include "UI/RetrieveUITheme.h"
#include "Settings/RetrieveSettingsSubsystem.h"
#include "Components/ProgressBar.h"
#include "Blueprint/WidgetTree.h"

namespace
{
	// 중첩된 자식 UserWidget 트리까지 들어가 모든 ProgressBar를 수집한다.
	void CollectProgressBars(UUserWidget* Root, TArray<UProgressBar*>& Out)
	{
		if (!Root || !Root->WidgetTree)
		{
			return;
		}
		Root->WidgetTree->ForEachWidget([&Out](UWidget* W)
		{
			if (UProgressBar* PB = Cast<UProgressBar>(W))
			{
				Out.Add(PB);
			}
			else if (UUserWidget* Nested = Cast<UUserWidget>(W))
			{
				CollectProgressBars(Nested, Out);
			}
		});
	}
}

void URetrieveThemedBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (URetrieveSettingsSubsystem* Subsystem = URetrieveSettingsSubsystem::Get(this))
	{
		Subsystem->OnSettingChanged.AddUniqueDynamic(this, &URetrieveThemedBarWidget::HandleSettingChanged);
	}
	ApplyThemeToBars();
	ApplyReduceMotion();
}

void URetrieveThemedBarWidget::NativeDestruct()
{
	if (URetrieveSettingsSubsystem* Subsystem = URetrieveSettingsSubsystem::Get(this))
	{
		Subsystem->OnSettingChanged.RemoveDynamic(this, &URetrieveThemedBarWidget::HandleSettingChanged);
	}
	Super::NativeDestruct();
}

void URetrieveThemedBarWidget::HandleSettingChanged(ERetrieveSettingsCategory Category)
{
	if (Category == ERetrieveSettingsCategory::Accessibility || Category == ERetrieveSettingsCategory::MAX)
	{
		ApplyThemeToBars();
		ApplyReduceMotion();
	}
}

void URetrieveThemedBarWidget::ApplyThemeToBars()
{
	TArray<UProgressBar*> Bars;
	CollectProgressBars(this, Bars);

	// 최초 1회 원본 색을 캐시(고대비 해제 시 복원).
	if (!bCachedOriginals)
	{
		for (UProgressBar* PB : Bars)
		{
			if (PB)
			{
				OriginalFillColors.Add(PB, PB->FillColorAndOpacity);
			}
		}
		bCachedOriginals = true;
	}

	const bool bHighContrast = URetrieveUISettingsLibrary::IsHighContrastEnabled();
	const URetrieveUITheme* Theme = URetrieveUISettingsLibrary::GetActiveUITheme();

	FLinearColor HighContrastColor = FLinearColor::White;
	if (Theme)
	{
		HighContrastColor = (BarColorRole == ERetrieveBarColorRole::Stamina) ? Theme->StaminaFill : Theme->HealthFill;
	}

	for (UProgressBar* PB : Bars)
	{
		if (!PB)
		{
			continue;
		}
		if (bHighContrast)
		{
			PB->SetFillColorAndOpacity(HighContrastColor);
		}
		else if (const FLinearColor* Original = OriginalFillColors.Find(PB))
		{
			PB->SetFillColorAndOpacity(*Original);
		}
	}
}

void URetrieveThemedBarWidget::ApplyReduceMotion()
{
	// Reduce Motion이 켜져 있으면 장식용 애니메이션(광택 sweep 등)을 정지한다.
	// 바 채움은 SetPercent로 갱신되므로 영향받지 않는다.
	// (해제는 위젯 재생성 시 자동 복원 — 접근성 설정은 게임 중 자주 토글되지 않음)
	if (URetrieveUISettingsLibrary::IsReduceMotionEnabled())
	{
		URetrieveUISettingsLibrary::StopAnimationsRecursive(this);
	}
}
