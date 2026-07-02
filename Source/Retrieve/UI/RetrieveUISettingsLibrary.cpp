#include "UI/RetrieveUISettingsLibrary.h"
#include "UI/RetrieveUITheme.h"
#include "Settings/RetrieveSettingsConfig.h"
#include "Settings/RetrieveGameUserSettings.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"

URetrieveUITheme* URetrieveUISettingsLibrary::GetActiveUITheme()
{
	const URetrieveSettingsConfig* Cfg = GetDefault<URetrieveSettingsConfig>();
	const URetrieveGameUserSettings* S = URetrieveGameUserSettings::Get();
	if (!Cfg)
	{
		return nullptr;
	}

	const bool bHighContrast = S && S->bHighContrastHUD;
	const TSoftObjectPtr<URetrieveUITheme>& Selected =
		(bHighContrast && !Cfg->HighContrastUITheme.IsNull()) ? Cfg->HighContrastUITheme : Cfg->DefaultUITheme;

	return Selected.LoadSynchronous();
}

float URetrieveUISettingsLibrary::GetUIScale()
{
	const URetrieveGameUserSettings* S = URetrieveGameUserSettings::Get();
	return S ? FMath::Clamp(S->UITextScale, 0.5f, 2.f) : 1.f;
}

bool URetrieveUISettingsLibrary::IsHighContrastEnabled()
{
	const URetrieveGameUserSettings* S = URetrieveGameUserSettings::Get();
	return S ? S->bHighContrastHUD : false;
}

bool URetrieveUISettingsLibrary::IsReduceMotionEnabled()
{
	const URetrieveGameUserSettings* S = URetrieveGameUserSettings::Get();
	return S ? S->bReduceMotion : false;
}

void URetrieveUISettingsLibrary::StopAnimationsRecursive(UUserWidget* Root)
{
	if (!Root)
	{
		return;
	}
	Root->StopAllAnimations();
	if (!Root->WidgetTree)
	{
		return;
	}
	Root->WidgetTree->ForEachWidget([](UWidget* W)
	{
		if (UUserWidget* Nested = Cast<UUserWidget>(W))
		{
			URetrieveUISettingsLibrary::StopAnimationsRecursive(Nested);
		}
	});
}
