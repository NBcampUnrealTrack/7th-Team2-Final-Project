#include "UI/Settings/RetrieveSettingRowToggle.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

void URetrieveSettingRowToggle::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_On)
	{
		Btn_On->OnClicked.AddUniqueDynamic(this, &URetrieveSettingRowToggle::HandleOnClicked);
	}
	if (Btn_Off)
	{
		Btn_Off->OnClicked.AddUniqueDynamic(this, &URetrieveSettingRowToggle::HandleOffClicked);
	}
	Refresh();
}

void URetrieveSettingRowToggle::SetValueSilently(bool bNewValue)
{
	TGuardValue<bool> Guard(bSuppressBroadcast, true);
	bCurrentValue = bNewValue;
	Refresh();
}

void URetrieveSettingRowToggle::SetLabelTexts(const FText& Label, const FText& Desc)
{
	if (LabelText) LabelText->SetText(Label);
	if (DescText)  DescText->SetText(Desc);
}

void URetrieveSettingRowToggle::Refresh()
{
	// 버튼 Style은 건드리지 않는다(WBP 디자인 유지) → 프리뷰/인게임 동일.
	// 선택 상태는 WBP의 프레임/배경 이미지 틴트로만 표현한다.
	const FLinearColor OnTint  = bCurrentValue  ? SelectedTint : UnselectedTint;
	const FLinearColor OffTint = !bCurrentValue ? SelectedTint : UnselectedTint;

	if (Img_Background_On)  Img_Background_On->SetColorAndOpacity(OnTint);
	if (Img_Frame_On)       Img_Frame_On->SetColorAndOpacity(OnTint);
	if (Img_Background_Off) Img_Background_Off->SetColorAndOpacity(OffTint);
	if (Img_Frame_Off)      Img_Frame_Off->SetColorAndOpacity(OffTint);
}

void URetrieveSettingRowToggle::HandleOnClicked()
{
	bCurrentValue = true;
	Refresh();
	if (!bSuppressBroadcast)
	{
		OnRowToggleChanged.Broadcast(RowKey, true);
	}
}

void URetrieveSettingRowToggle::HandleOffClicked()
{
	bCurrentValue = false;
	Refresh();
	if (!bSuppressBroadcast)
	{
		OnRowToggleChanged.Broadcast(RowKey, false);
	}
}
