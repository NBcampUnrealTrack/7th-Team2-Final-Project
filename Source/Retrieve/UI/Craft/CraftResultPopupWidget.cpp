#include "UI/Craft/CraftResultPopupWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

const FLinearColor UCraftResultPopupWidget::SuccessColor(0.90f, 0.75f, 0.30f, 1.0f);
const FLinearColor UCraftResultPopupWidget::FailureColor(0.85f, 0.28f, 0.24f, 1.0f);

void UCraftResultPopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Confirm && !Button_Confirm->OnClicked.IsBound())
	{
		Button_Confirm->OnClicked.AddDynamic(this, &UCraftResultPopupWidget::HandleConfirmClicked);
	}
}

void UCraftResultPopupWidget::ShowResult(bool bSuccess, UTexture2D* Icon)
{
	if (Image_ResultIcon && Icon)
	{
		Image_ResultIcon->SetBrushFromTexture(Icon, true);
	}

	if (Text_ResultTitle)
	{
		Text_ResultTitle->SetText(bSuccess
			? NSLOCTEXT("CraftResultPopup", "Success", "강화 성공!")
			: NSLOCTEXT("CraftResultPopup", "Failure", "강화 실패..."));
		Text_ResultTitle->SetColorAndOpacity(FSlateColor(bSuccess ? SuccessColor : FailureColor));
	}

	SetVisibility(ESlateVisibility::Visible);
}

void UCraftResultPopupWidget::HideResult()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

void UCraftResultPopupWidget::HandleConfirmClicked()
{
	HideResult();
}
