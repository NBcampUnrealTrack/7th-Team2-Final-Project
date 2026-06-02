#include "UI/HUD/RetrieveQuickSlotEntryWidget.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/DataTable.h"

void URetrieveQuickSlotEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Image_Icon)
	{
		EmptySlotBrush = Image_Icon->GetBrush();
		if (EmptySlotBrush.GetResourceObject() == nullptr)
		{
			EmptySlotBrush.DrawAs = ESlateBrushDrawType::Box;
			EmptySlotBrush.TintColor = FSlateColor(FLinearColor(0.04f, 0.04f, 0.04f, 0.72f));
		}
		bHasCachedEmptySlotBrush = true;
	}

	ShowEmptySlot();
}

void URetrieveQuickSlotEntryWidget::InitSlot(int32 InSlotKey, FName InItemId, UDataTable* InIconTable)
{
	NativeSlotKey = InSlotKey;
	if (UTextBlock* TextKey = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_Key"))))
	{
		TextKey->SetText(FText::AsNumber(NativeSlotKey));
	}

	if (InIconTable)
	{
		QuickSlotIconTable = InIconTable;
	}
	RefreshSlot(InItemId);
}

void URetrieveQuickSlotEntryWidget::RefreshSlot(FName ItemId)
{
	NativeCurrentItemId = ItemId;

	// 빈 슬롯 — 아이콘/수량 숨김
	if (ItemId.IsNone())
	{
		ShowEmptySlot();
		return;
	}

	if (!Image_Icon)
	{
		return;
	}

	if (!QuickSlotIconTable)
	{
		QuickSlotIconTable = LoadObject<UDataTable>(
			nullptr,
			TEXT("/Game/Retrieve/Data/Items/DT_ItemIcon.DT_ItemIcon"));
	}

	// 아이콘 테이블에서 텍스처 조회
	if (QuickSlotIconTable)
	{
		const FRetrieveItemIconRow* Row =
			QuickSlotIconTable->FindRow<FRetrieveItemIconRow>(ItemId, TEXT("QuickSlotEntry::RefreshSlot"));

		if (Row)
		{
			if (UTexture2D* Icon = Row->IconTexture.LoadSynchronous())
			{
				Image_Icon->SetBrushFromTexture(Icon, true);
				Image_Icon->SetVisibility(ESlateVisibility::Visible);
			}
			else
			{
				ShowEmptySlot();
			}
		}
		else
		{
			ShowEmptySlot();
		}
	}
	else
	{
		ShowEmptySlot();
	}
}

void URetrieveQuickSlotEntryWidget::ShowEmptySlot()
{
	SetVisibility(ESlateVisibility::HitTestInvisible);

	if (Image_Icon)
	{
		if (bHasCachedEmptySlotBrush)
		{
			Image_Icon->SetBrush(EmptySlotBrush);
		}
		Image_Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	if (Text_Count)
	{
		Text_Count->SetText(FText::GetEmpty());
		Text_Count->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void URetrieveQuickSlotEntryWidget::RefreshCount(int32 Count)
{
	if (!Text_Count)
	{
		return;
	}

	// 1개 이하면 수량 숨김, 2개 이상이면 숫자 표시
	if (Count > 1)
	{
		Text_Count->SetText(FText::AsNumber(Count));
		Text_Count->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		Text_Count->SetText(FText::GetEmpty());
		Text_Count->SetVisibility(ESlateVisibility::Collapsed);
	}
}
