#include "UI/Skill/RetrieveResonanceEntryWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"

void URetrieveResonanceEntryWidget::SetEntry(const FRetrieveResonanceEntryView& InView)
{
	if (Text_Name)
	{
		Text_Name->SetText(InView.DisplayName);
	}
	if (Text_Stacks)
	{
		Text_Stacks->SetText(InView.StacksText);
	}
	if (Text_Effect)
	{
		Text_Effect->SetText(InView.EffectText);
	}
	if (Image_Icon)
	{
		if (InView.Icon)
		{
			Image_Icon->SetBrushFromTexture(InView.Icon);
			Image_Icon->SetColorAndOpacity(InView.IconTint);
			Image_Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			// 아이콘이 없으면 브러시가 비어 빈 사각형이 남지 않도록 접는다.
			Image_Icon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}
