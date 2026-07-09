#include "UI/Menu/RetrieveLoadGameWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Texture2D.h"
#include "UObject/UnrealType.h"
#include "ImageUtils.h"
#include "Player/RetrievePlayerController.h"
#include "Save/RetrieveSaveGame.h"
#include "Save/RetrieveSaveSubsystem.h"

namespace
{
	UTexture2D* LoadFantasyMenuTexture(const TCHAR* AssetName)
	{
		const FString Path = FString::Printf(
			TEXT("/Game/External/UIFantasyWarriorMenu/Textures/FantasyMenus/%s.%s"),
			AssetName, AssetName);
		return LoadObject<UTexture2D>(nullptr, *Path);
	}

	FString ResolveBonfireDisplayName(const URetrieveSaveGame* SaveGame)
	{
		if (!SaveGame)
		{
			return FString();
		}
		return !SaveGame->BonfireDisplayName.IsEmpty()
			? SaveGame->BonfireDisplayName
			: SaveGame->LoadSnapshot.BonfireId.ToString();
	}

	/** PNG 바이트를 텍스처로 디코딩하고, 알파 0으로 저장된 구형 썸네일을 즉시 마이그레이션한다. */
	UTexture2D* DecodeThumbnail(const TArray<uint8>& Png)
	{
		if (Png.IsEmpty())
		{
			return nullptr;
		}

		UTexture2D* Texture = FImageUtils::ImportBufferAsTexture2D(Png);
		if (!Texture)
		{
			return nullptr;
		}

		if (FTexturePlatformData* PlatformData = Texture->GetPlatformData();
			PlatformData && PlatformData->PixelFormat == PF_B8G8R8A8 && !PlatformData->Mips.IsEmpty())
		{
			FTexture2DMipMap& Mip = PlatformData->Mips[0];
			if (FColor* MipPixels = static_cast<FColor*>(Mip.BulkData.Lock(LOCK_READ_WRITE)))
			{
				const int64 PixelCount = static_cast<int64>(Mip.SizeX) * Mip.SizeY;
				for (int64 PixelIndex = 0; PixelIndex < PixelCount; ++PixelIndex)
				{
					MipPixels[PixelIndex].A = 255;
				}
				Mip.BulkData.Unlock();
				Texture->UpdateResource();
			}
		}
		Texture->NeverStream = true;
		return Texture;
	}
}

void URetrieveLoadGameWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ScrollBox_LoadSlots = Cast<UScrollBox>(GetWidgetFromName(TEXT("ScrollBox_LoadSlots")));
	Image_PreviewThumbnail = Cast<UImage>(GetWidgetFromName(TEXT("Image_PreviewThumbnail")));
	Text_PreviewBonfireName = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_PreviewBonfireName")));
	Text_PreviewTimestamp = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_PreviewTimestamp")));
	// WBP_LoadGame에 배치된 위젯을 사용한다. 부모가 CanvasPanel이라 여기서 동적 생성하면
	// 슬롯 설정 없이 (0,0)에 붙어 화면 좌상단에 떠버리므로 동적 생성 폴백은 두지 않는다.
	Text_PreviewQuestName = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_PreviewQuestName")));

	if (UButton* CloseButton = Cast<UButton>(GetWidgetFromName(TEXT("Button_Close"))))
	{
		CloseButton->OnClicked.AddDynamic(this, &URetrieveLoadGameWidget::HandleCloseButtonClicked);
	}

	RefreshSlotEntries();
}

void URetrieveLoadGameWidget::NativeDestruct()
{
	SlotThumbnailTextures.Reset();
	PreviewThumbnailTexture = nullptr;
	Super::NativeDestruct();
}

void URetrieveLoadGameWidget::RefreshSlotEntries()
{
	if (!ScrollBox_LoadSlots)
	{
		return;
	}

	SlotThumbnailTextures.Reset();
	// WBP_SaveSlotEntry의 SlotIndex 변수는 인스턴스 편집이 불가능해 디자이너에서 개별 지정할 수 없으므로,
	// ScrollBox 내 배치 순서(ChildIndex)를 슬롯 번호의 유일한 근거로 삼는다(0~MaxSaveSlots-1 순서로 배치 필요).
	for (int32 ChildIndex = 0; ChildIndex < ScrollBox_LoadSlots->GetChildrenCount(); ++ChildIndex)
	{
		if (UUserWidget* Entry = Cast<UUserWidget>(ScrollBox_LoadSlots->GetChildAt(ChildIndex)))
		{
			ApplyThumbnailToEntry(Entry, ChildIndex);
			BindEntryInteractions(Entry, ChildIndex);
		}
	}

	UpdatePreview(0);
}

void URetrieveLoadGameWidget::ApplyThumbnailToEntry(UUserWidget* EntryWidget, int32 SlotIndex)
{
	if (!EntryWidget || !EntryWidget->WidgetTree)
	{
		return;
	}

	UHorizontalBox* Content = Cast<UHorizontalBox>(EntryWidget->GetWidgetFromName(TEXT("HorizontalBox_Content")));
	if (!Content)
	{
		return;
	}

	UImage* ThumbnailImage = Cast<UImage>(EntryWidget->GetWidgetFromName(TEXT("Image_SaveThumbnail")));
	if (!ThumbnailImage)
	{
		UWidget* InfoColumn = EntryWidget->GetWidgetFromName(TEXT("VerticalBox_Info"));
		if (InfoColumn)
		{
			Content->RemoveChild(InfoColumn);
		}

		USizeBox* ThumbnailBox = EntryWidget->WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), TEXT("SizeBox_LoadThumbnail"));
		ThumbnailBox->SetWidthOverride(160.0f);
		ThumbnailBox->SetHeightOverride(90.0f);
		ThumbnailImage = EntryWidget->WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(), TEXT("Image_SaveThumbnail"));
		ThumbnailImage->SetColorAndOpacity(FLinearColor(0.82f, 0.92f, 1.0f, 1.0f));
		ThumbnailImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		ThumbnailBox->AddChild(ThumbnailImage);

		if (UHorizontalBoxSlot* ThumbSlot = Content->AddChildToHorizontalBox(ThumbnailBox))
		{
			ThumbSlot->SetPadding(FMargin(8.0f, 6.0f, 18.0f, 6.0f));
			ThumbSlot->SetVerticalAlignment(VAlign_Center);
		}
		if (InfoColumn)
		{
			if (UHorizontalBoxSlot* InfoSlot = Content->AddChildToHorizontalBox(InfoColumn))
			{
				InfoSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				InfoSlot->SetVerticalAlignment(VAlign_Center);
			}
		}
	}

	URetrieveSaveSubsystem* SaveSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<URetrieveSaveSubsystem>()
		: nullptr;
	URetrieveSaveGame* SaveGame = SaveSubsystem ? SaveSubsystem->GetSaveGameForSlot(SlotIndex) : nullptr;

	if (UButton* EntryButton = Cast<UButton>(EntryWidget->GetWidgetFromName(TEXT("Button"))))
	{
		EntryButton->SetIsEnabled(SaveGame != nullptr);
	}

	if (UTextBlock* BonfireNameText = Cast<UTextBlock>(EntryWidget->GetWidgetFromName(TEXT("Text_BonfireName"))))
	{
		BonfireNameText->SetText(SaveGame
			? FText::FromString(ResolveBonfireDisplayName(SaveGame))
			: NSLOCTEXT("RetrieveLoadGame", "EmptySlot", "빈 슬롯"));
	}

	if (UTextBlock* TimestampText = Cast<UTextBlock>(EntryWidget->GetWidgetFromName(TEXT("Text_Timestamp"))))
	{
		TimestampText->SetText(SaveGame ? FText::FromString(SaveGame->SaveTimestamp) : FText::GetEmpty());
	}

	// 저장 시점 추적 퀘스트 표시. WBP 에셋을 건드리지 않기 위해 썸네일과 같은 방식으로 동적 생성한다.
	UTextBlock* QuestNameText = Cast<UTextBlock>(EntryWidget->GetWidgetFromName(TEXT("Text_QuestName")));
	if (!QuestNameText)
	{
		if (UVerticalBox* InfoBox = Cast<UVerticalBox>(EntryWidget->GetWidgetFromName(TEXT("VerticalBox_Info"))))
		{
			QuestNameText = EntryWidget->WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(), TEXT("Text_QuestName"));
			QuestNameText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.78f, 0.55f, 1.0f)));
			QuestNameText->SetAutoWrapText(true);
			if (UVerticalBoxSlot* QuestSlot = InfoBox->AddChildToVerticalBox(QuestNameText))
			{
				QuestSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
			}
		}
	}
	if (QuestNameText)
	{
		const FString QuestLine = (SaveGame && !SaveGame->TrackedQuestName.IsEmpty())
			? (SaveGame->TrackedQuestObjective.IsEmpty()
				? SaveGame->TrackedQuestName
				: FString::Printf(TEXT("%s - %s"), *SaveGame->TrackedQuestName, *SaveGame->TrackedQuestObjective))
			: FString();
		QuestNameText->SetText(FText::FromString(QuestLine));
		QuestNameText->SetVisibility(QuestLine.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	if (UTexture2D* Texture = SaveGame ? DecodeThumbnail(SaveGame->ScreenshotPng) : nullptr)
	{
		SlotThumbnailTextures.Add(Texture);
		ThumbnailImage->SetBrushFromTexture(Texture, true);
		ThumbnailImage->SetColorAndOpacity(FLinearColor::White);
		return;
	}

	ThumbnailImage->SetBrushFromTexture(nullptr);
	ThumbnailImage->SetColorAndOpacity(FLinearColor(0.025f, 0.08f, 0.12f, 0.92f));
}

void URetrieveLoadGameWidget::BindEntryInteractions(UUserWidget* EntryWidget, int32 SlotIndex)
{
	UButton* EntryButton = EntryWidget ? Cast<UButton>(EntryWidget->GetWidgetFromName(TEXT("Button"))) : nullptr;
	if (!EntryButton)
	{
		return;
	}

	switch (SlotIndex)
	{
	case 0:
		EntryButton->OnClicked.AddDynamic(this, &URetrieveLoadGameWidget::HandleSlot0Clicked);
		EntryButton->OnHovered.AddDynamic(this, &URetrieveLoadGameWidget::HandleSlot0Hovered);
		break;
	case 1:
		EntryButton->OnClicked.AddDynamic(this, &URetrieveLoadGameWidget::HandleSlot1Clicked);
		EntryButton->OnHovered.AddDynamic(this, &URetrieveLoadGameWidget::HandleSlot1Hovered);
		break;
	case 2:
		EntryButton->OnClicked.AddDynamic(this, &URetrieveLoadGameWidget::HandleSlot2Clicked);
		EntryButton->OnHovered.AddDynamic(this, &URetrieveLoadGameWidget::HandleSlot2Hovered);
		break;
	case 3:
		EntryButton->OnClicked.AddDynamic(this, &URetrieveLoadGameWidget::HandleSlot3Clicked);
		EntryButton->OnHovered.AddDynamic(this, &URetrieveLoadGameWidget::HandleSlot3Hovered);
		break;
	case 4:
		EntryButton->OnClicked.AddDynamic(this, &URetrieveLoadGameWidget::HandleSlot4Clicked);
		EntryButton->OnHovered.AddDynamic(this, &URetrieveLoadGameWidget::HandleSlot4Hovered);
		break;
	default:
		break;
	}
}

void URetrieveLoadGameWidget::UpdatePreview(int32 SlotIndex)
{
	URetrieveSaveSubsystem* SaveSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<URetrieveSaveSubsystem>()
		: nullptr;
	URetrieveSaveGame* SaveGame = SaveSubsystem ? SaveSubsystem->GetSaveGameForSlot(SlotIndex) : nullptr;

	if (Text_PreviewBonfireName)
	{
		Text_PreviewBonfireName->SetText(SaveGame
			? FText::FromString(ResolveBonfireDisplayName(SaveGame))
			: NSLOCTEXT("RetrieveLoadGame", "EmptySlotPreview", "빈 슬롯"));
	}
	if (Text_PreviewTimestamp)
	{
		Text_PreviewTimestamp->SetText(SaveGame ? FText::FromString(SaveGame->SaveTimestamp) : FText::GetEmpty());
	}
	if (Text_PreviewQuestName)
	{
		const FString QuestLine = (SaveGame && !SaveGame->TrackedQuestName.IsEmpty())
			? (SaveGame->TrackedQuestObjective.IsEmpty()
				? SaveGame->TrackedQuestName
				: FString::Printf(TEXT("%s - %s"), *SaveGame->TrackedQuestName, *SaveGame->TrackedQuestObjective))
			: FString();
		Text_PreviewQuestName->SetText(FText::FromString(QuestLine));
		Text_PreviewQuestName->SetVisibility(QuestLine.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	PreviewThumbnailTexture = SaveGame ? DecodeThumbnail(SaveGame->ScreenshotPng) : nullptr;
	if (Image_PreviewThumbnail)
	{
		if (PreviewThumbnailTexture)
		{
			Image_PreviewThumbnail->SetBrushFromTexture(PreviewThumbnailTexture, true);
			Image_PreviewThumbnail->SetColorAndOpacity(FLinearColor::White);
		}
		else
		{
			Image_PreviewThumbnail->SetBrushFromTexture(nullptr);
			Image_PreviewThumbnail->SetColorAndOpacity(FLinearColor(0.025f, 0.08f, 0.12f, 0.92f));
		}
	}
}

void URetrieveLoadGameWidget::RequestLoadSlot(int32 SlotIndex)
{
	URetrieveSaveSubsystem* SaveSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<URetrieveSaveSubsystem>()
		: nullptr;
	if (!SaveSubsystem || !SaveSubsystem->GetSaveGameForSlot(SlotIndex))
	{
		return;
	}

	HighlightClickedEntry(SlotIndex);

	if (ARetrievePlayerController* PC = GetRetrievePlayerController())
	{
		PC->RequestLoadGameSlot(SlotIndex);
	}
}

void URetrieveLoadGameWidget::HighlightClickedEntry(int32 SlotIndex)
{
	if (!ScrollBox_LoadSlots)
	{
		return;
	}

	// 클릭 선택 글로우는 호버(흰색)와 구분되는 골드 틴트. (BonfireMenuWidget과 동일 규칙)
	const FLinearColor SelectedGlowTint(1.0f, 0.72f, 0.30f, 1.0f);

	for (int32 ChildIndex = 0; ChildIndex < ScrollBox_LoadSlots->GetChildrenCount(); ++ChildIndex)
	{
		UUserWidget* Entry = Cast<UUserWidget>(ScrollBox_LoadSlots->GetChildAt(ChildIndex));
		if (!Entry)
		{
			continue;
		}

		const bool bIsSelected = ChildIndex == SlotIndex;

		// 언호버(SetSelected(false))가 선택 글로우를 끄지 않도록 하는 WBP 플래그.
		if (FBoolProperty* ClickSelectedProperty =
			FindFProperty<FBoolProperty>(Entry->GetClass(), TEXT("bIsClickSelected")))
		{
			ClickSelectedProperty->SetPropertyValue_InContainer(Entry, bIsSelected);
		}

		const FLinearColor GlowTint = bIsSelected ? SelectedGlowTint : FLinearColor::White;
		for (const TCHAR* GlowWidgetName : { TEXT("Img_SelectedGlow"), TEXT("Img_EdgeGlow"), TEXT("IMG_Arrow_1") })
		{
			if (UImage* GlowImage = Cast<UImage>(Entry->GetWidgetFromName(GlowWidgetName)))
			{
				GlowImage->SetColorAndOpacity(GlowTint);
			}
		}

		if (UFunction* SetSelectedFunction = Entry->FindFunction(TEXT("SetSelected")))
		{
			struct FSetSelectedParams
			{
				bool bIsSelected = false;
			};

			FSetSelectedParams Params;
			Params.bIsSelected = bIsSelected;
			Entry->ProcessEvent(SetSelectedFunction, &Params);
		}
	}
}

ARetrievePlayerController* URetrieveLoadGameWidget::GetRetrievePlayerController() const
{
	return Cast<ARetrievePlayerController>(GetOwningPlayer());
}

void URetrieveLoadGameWidget::HandleSlot0Clicked() { RequestLoadSlot(0); }
void URetrieveLoadGameWidget::HandleSlot1Clicked() { RequestLoadSlot(1); }
void URetrieveLoadGameWidget::HandleSlot2Clicked() { RequestLoadSlot(2); }
void URetrieveLoadGameWidget::HandleSlot3Clicked() { RequestLoadSlot(3); }
void URetrieveLoadGameWidget::HandleSlot4Clicked() { RequestLoadSlot(4); }

void URetrieveLoadGameWidget::HandleSlot0Hovered() { UpdatePreview(0); }
void URetrieveLoadGameWidget::HandleSlot1Hovered() { UpdatePreview(1); }
void URetrieveLoadGameWidget::HandleSlot2Hovered() { UpdatePreview(2); }
void URetrieveLoadGameWidget::HandleSlot3Hovered() { UpdatePreview(3); }
void URetrieveLoadGameWidget::HandleSlot4Hovered() { UpdatePreview(4); }

void URetrieveLoadGameWidget::HandleCloseButtonClicked()
{
	RequestClose();
}
