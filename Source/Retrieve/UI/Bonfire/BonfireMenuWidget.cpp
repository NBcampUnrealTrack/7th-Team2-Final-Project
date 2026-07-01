#include "UI/Bonfire/BonfireMenuWidget.h"

#include "Components/Button.h"
#include "Components/Inventory/InventoryComponent.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "Save/RetrieveSaveGame.h"
#include "Save/RetrieveSaveSubsystem.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"
#include "GameFramework/Pawn.h"
#include "UI/Craft/CraftPanelWidget.h"
#include "World/RetrieveBonfireActor.h"
#include "EngineUtils.h"

namespace
{
	UTexture2D* LoadFantasyMenuTexture(const TCHAR* AssetName)
	{
		const FString Path = FString::Printf(
			TEXT("/Game/External/UIFantasyWarriorMenu/Textures/FantasyMenus/%s.%s"),
			AssetName, AssetName);
		return LoadObject<UTexture2D>(nullptr, *Path);
	}

	FString ResolveBonfireDisplayName(UWorld* World, FName BonfireId, const FString& StoredDisplayName = FString())
	{
		const FString BonfireIdString = BonfireId.ToString();
		if (!StoredDisplayName.IsEmpty() && !StoredDisplayName.Equals(BonfireIdString, ESearchCase::CaseSensitive))
		{
			return StoredDisplayName;
		}

		if (World && !BonfireId.IsNone())
		{
			for (TActorIterator<ARetrieveBonfireActor> It(World); It; ++It)
			{
				if (It->BonfireId == BonfireId && !It->DisplayName.IsEmpty())
				{
					return It->DisplayName.ToString();
				}
			}
		}

		return !StoredDisplayName.IsEmpty() ? StoredDisplayName : BonfireIdString;
	}

}

void UBonfireMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyFantasyMenuStyle();
	RuntimeButtonLoad = Cast<UButton>(GetWidgetFromName(TEXT("Button_Load")));
	ButtonBackground_TabSave = Cast<UImage>(GetWidgetFromName(TEXT("IMG_ButtonBackground_TabSave")));
	ButtonBackground_TabCraft = Cast<UImage>(GetWidgetFromName(TEXT("IMG_ButtonBackground_TabCraft")));
	ButtonBackground_Save = Cast<UImage>(GetWidgetFromName(TEXT("IMG_ButtonBackground_Save")));
	ButtonBackground_Load = Cast<UImage>(GetWidgetFromName(TEXT("IMG_ButtonBackground_Load")));

	// BP Construct/RefreshSaveInfo가 슬롯 엔트리를 생성하는 시점과 무관하게 썸네일을 주입한다.
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (URetrieveSaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<URetrieveSaveSubsystem>())
		{
			SaveSubsystem->OnSaveCompleted.AddUniqueDynamic(this, &UBonfireMenuWidget::HandleSaveCompleted);
		}
	}
	QueueThumbnailRefresh(5);
	// 버튼 스타일은 UMG/블루프린트에서 설정한 값을 사용하도록 런타임 강제 적용을 비활성화합니다.

	UCraftPanelWidget* ResolvedCraftPanel = Cast<UCraftPanelWidget>(GetWidgetFromName(TEXT("WBP_CraftPanel")));
	if (!ResolvedCraftPanel)
	{
		ResolvedCraftPanel = Cast<UCraftPanelWidget>(GetWidgetFromName(TEXT("CraftPanel")));
	}

	if (!ResolvedCraftPanel)
	{
		UE_LOG(LogTemp, Warning, TEXT("BonfireMenuWidget: CraftPanel is not bound. Check WBP_BonfireMenu has a UCraftPanelWidget named WBP_CraftPanel or CraftPanel."));
		return;
	}

	ResolvedCraftPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UVerticalBoxSlot* CraftPanelSlot = Cast<UVerticalBoxSlot>(ResolvedCraftPanel->Slot))
	{
		CraftPanelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	if (Button_TabSave)
	{
		Button_TabSave->OnClicked.AddUniqueDynamic(this, &UBonfireMenuWidget::HandleTabSaveClicked);
	}
	if (Button_TabCraft)
	{
		Button_TabCraft->OnClicked.AddUniqueDynamic(this, &UBonfireMenuWidget::HandleTabCraftClicked);
	}
	// 초기 활성 탭: 실제로 보이는 패널 기준으로 결정 (제작 패널이 보이면 제작 활성)
	const bool bCraftVisible = Panel_Craft
		&& Panel_Craft->GetVisibility() != ESlateVisibility::Collapsed
		&& Panel_Craft->GetVisibility() != ESlateVisibility::Hidden;
	SetActiveTab(!bCraftVisible);

	APawn* OwningPawn = GetOwningPlayerPawn();
	UInventoryComponent* InventoryComponent = OwningPawn
		? OwningPawn->FindComponentByClass<UInventoryComponent>()
		: nullptr;
	if (InventoryComponent)
	{
		ResolvedCraftPanel->InitializeCraftPanel(InventoryComponent);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("BonfireMenuWidget: Owning pawn has no InventoryComponent. Craft recipe list cannot be initialized."));
	}
	if (Button_Save)
	{
		// BP 저장 그래프가 현재 SaveToSlot까지 도달하지 않는 경우에도 확실히 저장되도록
		// 저장/덮어쓰기 흐름은 네이티브에서 단일 소유한다.
		Button_Save->OnClicked.Clear();
		Button_Save->OnClicked.AddUniqueDynamic(this, &UBonfireMenuWidget::HandleSaveButtonClicked);
	}
	if (UButton* ConfirmButton = Cast<UButton>(GetWidgetFromName(TEXT("Btn_ConfirmOverwrite"))))
	{
		ConfirmButton->OnClicked.Clear();
		ConfirmButton->OnClicked.AddUniqueDynamic(this, &UBonfireMenuWidget::HandleConfirmOverwriteClicked);
	}
	if (UButton* CancelButton = Cast<UButton>(GetWidgetFromName(TEXT("Btn_CancelOverwrite"))))
	{
		CancelButton->OnClicked.Clear();
		CancelButton->OnClicked.AddUniqueDynamic(this, &UBonfireMenuWidget::HandleCancelOverwriteClicked);
	}

}

void UBonfireMenuWidget::ApplyFantasyMenuStyle()
{
	UTexture2D* Background = LoadFantasyMenuTexture(TEXT("SPR_FantasyMenus_Frame_Box_Large_01_Background"));

	if (UImage* SaveBackground = Cast<UImage>(GetWidgetFromName(TEXT("IMG_SaveListBackground"))))
	{
		SaveBackground->SetBrushFromTexture(Background, true);
		SaveBackground->SetColorAndOpacity(FLinearColor(0.025f, 0.12f, 0.19f, 0.96f));
	}

}

void UBonfireMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateStyledButtonBackground(Button_TabSave, ButtonBackground_TabSave, bSaveTabActive);
	UpdateStyledButtonBackground(Button_TabCraft, ButtonBackground_TabCraft, !bSaveTabActive);
	UpdateStyledButtonBackground(Button_Save, ButtonBackground_Save, false);
	UpdateStyledButtonBackground(RuntimeButtonLoad, ButtonBackground_Load, false);
	UpdateSlotSelectionVisuals();
}

void UBonfireMenuWidget::UpdateStyledButtonBackground(UButton* Button, UImage* Background, bool bSelected) const
{
	if (!Button || !Background)
	{
		return;
	}

	FLinearColor Tint(0.018f, 0.075f, 0.12f, 0.96f);
	if (!Button->GetIsEnabled())
	{
		Tint = FLinearColor(0.16f, 0.17f, 0.19f, 0.55f);
	}
	else if (Button->IsPressed())
	{
		Tint = FLinearColor(0.025f, 0.19f, 0.36f, 1.0f);
	}
	else if (Button->IsHovered())
	{
		Tint = FLinearColor(0.055f, 0.34f, 0.62f, 1.0f);
	}
	else if (bSelected)
	{
		Tint = FLinearColor(0.045f, 0.28f, 0.56f, 1.0f);
	}

	Background->SetColorAndOpacity(Tint);
}

void UBonfireMenuWidget::HandleSaveButtonClicked()
{
	int32 SelectedSlotIndex = 0;
	if (const FIntProperty* SelectedProperty = FindFProperty<FIntProperty>(GetClass(), TEXT("SelectedSlotIndex")))
	{
		SelectedSlotIndex = SelectedProperty->GetPropertyValue_InContainer(this);
	}
	SelectedSlotIndex = FMath::Clamp(SelectedSlotIndex, 0, URetrieveSaveSubsystem::MaxSaveSlots - 1);

	URetrieveSaveSubsystem* SaveSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<URetrieveSaveSubsystem>()
		: nullptr;
	if (!SaveSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("[BonfireMenu] 저장 실패: SaveSubsystem 없음"));
		return;
	}

	if (SaveSubsystem->GetSaveGameForSlot(SelectedSlotIndex))
	{
		if (UWidget* ConfirmPanel = GetWidgetFromName(TEXT("Panel_ConfirmOverwrite")))
		{
			ConfirmPanel->SetVisibility(ESlateVisibility::Visible);
			return;
		}
	}

	PerformSelectedSlotSave();
}

void UBonfireMenuWidget::HandleConfirmOverwriteClicked()
{
	PerformSelectedSlotSave();
	if (UWidget* ConfirmPanel = GetWidgetFromName(TEXT("Panel_ConfirmOverwrite")))
	{
		ConfirmPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UBonfireMenuWidget::HandleCancelOverwriteClicked()
{
	if (UWidget* ConfirmPanel = GetWidgetFromName(TEXT("Panel_ConfirmOverwrite")))
	{
		ConfirmPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UBonfireMenuWidget::PerformSelectedSlotSave()
{
	int32 SelectedSlotIndex = 0;
	if (const FIntProperty* SelectedProperty = FindFProperty<FIntProperty>(GetClass(), TEXT("SelectedSlotIndex")))
	{
		SelectedSlotIndex = SelectedProperty->GetPropertyValue_InContainer(this);
	}
	SelectedSlotIndex = FMath::Clamp(SelectedSlotIndex, 0, URetrieveSaveSubsystem::MaxSaveSlots - 1);

	FName ResolvedBonfireId = BonfireId;
	if (ResolvedBonfireId.IsNone())
	{
		APawn* Pawn = GetOwningPlayerPawn();
		float BestDistanceSquared = TNumericLimits<float>::Max();
		for (TActorIterator<ARetrieveBonfireActor> It(GetWorld()); It; ++It)
		{
			const float DistanceSquared = Pawn
				? FVector::DistSquared(Pawn->GetActorLocation(), It->GetActorLocation())
				: 0.0f;
			if (DistanceSquared < BestDistanceSquared)
			{
				BestDistanceSquared = DistanceSquared;
				ResolvedBonfireId = It->BonfireId.IsNone() ? It->GetFName() : It->BonfireId;
			}
		}
	}

	const FString DisplayName = ResolveBonfireDisplayName(GetWorld(), ResolvedBonfireId);

	URetrieveSaveSubsystem* SaveSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<URetrieveSaveSubsystem>()
		: nullptr;
	const bool bSaved = SaveSubsystem && !ResolvedBonfireId.IsNone()
		&& SaveSubsystem->SaveToSlot(GetOwningPlayer(), ResolvedBonfireId, SelectedSlotIndex, DisplayName);
	if (bSaved)
	{
		UE_LOG(LogTemp, Log, TEXT("[BonfireMenu] 슬롯 저장 성공 Slot=%d BonfireId=%s"),
			SelectedSlotIndex, *ResolvedBonfireId.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[BonfireMenu] 슬롯 저장 실패 Slot=%d BonfireId=%s"),
			SelectedSlotIndex, *ResolvedBonfireId.ToString());
	}

	if (bSaved)
	{
		if (UFunction* RefreshFunction = FindFunction(TEXT("RefreshSaveInfo")))
		{
			ProcessEvent(RefreshFunction, nullptr);
		}
	}
}

void UBonfireMenuWidget::NativeDestruct()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (URetrieveSaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<URetrieveSaveSubsystem>())
		{
			SaveSubsystem->OnSaveCompleted.RemoveDynamic(this, &UBonfireMenuWidget::HandleSaveCompleted);
		}
	}
	SlotThumbnailTextures.Reset();
	RuntimeButtonLoad = nullptr;
	ButtonBackground_TabSave = nullptr;
	ButtonBackground_TabCraft = nullptr;
	ButtonBackground_Save = nullptr;
	ButtonBackground_Load = nullptr;
	ThumbnailRefreshAttemptsRemaining = 0;
	LastAppliedSelectedSlotIndex = INDEX_NONE;
	LastAppliedSlotEntryCount = INDEX_NONE;
	Super::NativeDestruct();
}

void UBonfireMenuWidget::HandleSaveCompleted()
{
	// BP가 저장 직후 엔트리를 재생성하므로 최종 트리가 생길 때까지 다시 확인한다.
	QueueThumbnailRefresh(5);
}

void UBonfireMenuWidget::QueueThumbnailRefresh(int32 AttemptCount)
{
	ThumbnailRefreshAttemptsRemaining = FMath::Max(
		ThumbnailRefreshAttemptsRemaining, FMath::Max(AttemptCount, 1));
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &UBonfireMenuWidget::RefreshSlotThumbnails));
	}
}

void UBonfireMenuWidget::RefreshSlotThumbnails()
{
	ThumbnailRefreshAttemptsRemaining = FMath::Max(ThumbnailRefreshAttemptsRemaining - 1, 0);
	UScrollBox* SaveSlots = Cast<UScrollBox>(GetWidgetFromName(TEXT("ScrollBox_SaveSlots")));
	if (!SaveSlots || SaveSlots->GetChildrenCount() == 0)
	{
		if (ThumbnailRefreshAttemptsRemaining > 0)
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimerForNextTick(
					FTimerDelegate::CreateUObject(this, &UBonfireMenuWidget::RefreshSlotThumbnails));
			}
		}
		return;
	}

	SlotThumbnailTextures.Reset();
	for (int32 ChildIndex = 0; ChildIndex < SaveSlots->GetChildrenCount(); ++ChildIndex)
	{
		if (UUserWidget* Entry = Cast<UUserWidget>(SaveSlots->GetChildAt(ChildIndex)))
		{
			ApplyThumbnailToEntry(Entry, ChildIndex);
		}
	}
	LastAppliedSelectedSlotIndex = INDEX_NONE;
	LastAppliedSlotEntryCount = INDEX_NONE;
	UpdateSlotSelectionVisuals();
}

void UBonfireMenuWidget::UpdateSlotSelectionVisuals()
{
	UScrollBox* SaveSlots = Cast<UScrollBox>(GetWidgetFromName(TEXT("ScrollBox_SaveSlots")));
	if (!SaveSlots)
	{
		return;
	}

	int32 SelectedSlotIndex = 0;
	if (const FIntProperty* SelectedProperty = FindFProperty<FIntProperty>(GetClass(), TEXT("SelectedSlotIndex")))
	{
		SelectedSlotIndex = SelectedProperty->GetPropertyValue_InContainer(this);
	}

	const int32 EntryCount = SaveSlots->GetChildrenCount();
	if (SelectedSlotIndex == LastAppliedSelectedSlotIndex && EntryCount == LastAppliedSlotEntryCount)
	{
		return;
	}

	for (int32 ChildIndex = 0; ChildIndex < EntryCount; ++ChildIndex)
	{
		UUserWidget* Entry = Cast<UUserWidget>(SaveSlots->GetChildAt(ChildIndex));
		if (!Entry)
		{
			continue;
		}

		if (UFunction* SetSelectedFunction = Entry->FindFunction(TEXT("SetSelected")))
		{
			struct FSetSelectedParams
			{
				bool bIsSelected = false;
			};

			FSetSelectedParams Params;
			Params.bIsSelected = ResolveSlotIndex(Entry, ChildIndex) == SelectedSlotIndex;
			Entry->ProcessEvent(SetSelectedFunction, &Params);
		}
	}

	LastAppliedSelectedSlotIndex = SelectedSlotIndex;
	LastAppliedSlotEntryCount = EntryCount;
}

int32 UBonfireMenuWidget::ResolveSlotIndex(const UUserWidget* EntryWidget, int32 FallbackSlotIndex)
{
	if (EntryWidget)
	{
		if (const FIntProperty* SlotProperty = FindFProperty<FIntProperty>(EntryWidget->GetClass(), TEXT("SlotIndex")))
		{
			return SlotProperty->GetPropertyValue_InContainer(EntryWidget);
		}
	}
	return FallbackSlotIndex;
}

void UBonfireMenuWidget::ApplyThumbnailToEntry(UUserWidget* EntryWidget, int32 FallbackSlotIndex)
{
	if (!EntryWidget || !EntryWidget->WidgetTree)
	{
		return;
	}

	UHorizontalBox* Content = Cast<UHorizontalBox>(EntryWidget->GetWidgetFromName(TEXT("HorizontalBox_Content")));
	if (!Content)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BonfireMenu] SaveSlotEntry에 HorizontalBox_Content가 없어 썸네일을 표시할 수 없습니다."));
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
			USizeBox::StaticClass(), TEXT("SizeBox_SaveThumbnail"));
		ThumbnailBox->SetWidthOverride(240.0f);
		ThumbnailBox->SetHeightOverride(135.0f);
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

	// 썸네일 위에 별도의 금색 장식 프레임을 겹쳐 참조 이미지와 같은 카드 형태로 만든다.
	if (ThumbnailImage && !EntryWidget->GetWidgetFromName(TEXT("Image_SaveThumbnailFrame")))
	{
		if (USizeBox* ThumbnailBox = Cast<USizeBox>(ThumbnailImage->GetParent()))
		{
			ThumbnailBox->RemoveChild(ThumbnailImage);
			UOverlay* ThumbnailOverlay = EntryWidget->WidgetTree->ConstructWidget<UOverlay>(
				UOverlay::StaticClass(), TEXT("Overlay_SaveThumbnail"));
			ThumbnailBox->AddChild(ThumbnailOverlay);
			if (UOverlaySlot* ImageSlot = ThumbnailOverlay->AddChildToOverlay(ThumbnailImage))
			{
				ImageSlot->SetHorizontalAlignment(HAlign_Fill);
				ImageSlot->SetVerticalAlignment(VAlign_Fill);
				ImageSlot->SetPadding(FMargin(8.0f));
			}

			if (UTexture2D* OrnateFrame = LoadFantasyMenuTexture(TEXT("SPR_FantasyMenus_Frame_Box_Large_01")))
			{
				UImage* ThumbnailFrame = EntryWidget->WidgetTree->ConstructWidget<UImage>(
					UImage::StaticClass(), TEXT("Image_SaveThumbnailFrame"));
				ThumbnailFrame->SetBrushFromTexture(OrnateFrame, true);
				ThumbnailFrame->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				if (UOverlaySlot* FrameSlot = ThumbnailOverlay->AddChildToOverlay(ThumbnailFrame))
				{
					FrameSlot->SetHorizontalAlignment(HAlign_Fill);
					FrameSlot->SetVerticalAlignment(VAlign_Fill);
				}
			}
		}
	}

	const int32 SlotIndex = ResolveSlotIndex(EntryWidget, FallbackSlotIndex);
	URetrieveSaveSubsystem* SaveSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<URetrieveSaveSubsystem>()
		: nullptr;
	URetrieveSaveGame* SaveGame = SaveSubsystem
		? SaveSubsystem->GetSaveGameForSlot(SlotIndex)
		: nullptr;

	if (SaveGame)
	{
		if (UTextBlock* BonfireNameText = Cast<UTextBlock>(EntryWidget->GetWidgetFromName(TEXT("Text_BonfireName"))))
		{
			BonfireNameText->SetText(FText::FromString(ResolveBonfireDisplayName(
				GetWorld(), SaveGame->LoadSnapshot.BonfireId, SaveGame->BonfireDisplayName)));
		}
	}

	if (SaveGame && !SaveGame->ScreenshotPng.IsEmpty())
	{
		if (UTexture2D* Texture = FImageUtils::ImportBufferAsTexture2D(SaveGame->ScreenshotPng))
		{
			// 알파 0으로 저장된 기존 슬롯 이미지도 즉시 정상 표시되도록 런타임 마이그레이션한다.
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
			SlotThumbnailTextures.Add(Texture);
			ThumbnailImage->SetBrushFromTexture(Texture, true);
			ThumbnailImage->SetColorAndOpacity(FLinearColor::White);
			UE_LOG(LogTemp, Log, TEXT("[BonfireMenu] 슬롯 %d 썸네일 표시 완료 (%d bytes)"),
				SlotIndex, SaveGame->ScreenshotPng.Num());
			return;
		}

		UE_LOG(LogTemp, Warning, TEXT("[BonfireMenu] 슬롯 %d PNG 디코딩 실패 (%d bytes)"),
			SlotIndex, SaveGame->ScreenshotPng.Num());
	}

	// 기존 저장 파일과 빈 슬롯은 어두운 플레이스홀더로 표시한다.
	ThumbnailImage->SetBrushFromTexture(nullptr);
	ThumbnailImage->SetColorAndOpacity(FLinearColor(0.025f, 0.08f, 0.12f, 0.92f));
}

void UBonfireMenuWidget::SetActiveTab(bool bSaveActive)
{
	bSaveTabActive = bSaveActive;
	// 버튼 배경색은 UMG/블루프린트 스타일을 유지합니다.
	// const FLinearColor ActiveButtonTint = FLinearColor::White;
	// const FLinearColor InactiveButtonTint(0.46f, 0.50f, 0.56f, 0.88f);

	if (Button_TabSave)
	{
		// Button_TabSave->SetBackgroundColor(bSaveActive ? ActiveButtonTint : InactiveButtonTint);
	}
	if (Button_TabCraft)
	{
		// Button_TabCraft->SetBackgroundColor(bSaveActive ? InactiveButtonTint : ActiveButtonTint);
	}
	if (Text_TabSave)
	{
		Text_TabSave->SetColorAndOpacity(FSlateColor(bSaveActive ? TabActiveTextColor : TabInactiveTextColor));
	}
	if (Text_TabCraft)
	{
		Text_TabCraft->SetColorAndOpacity(FSlateColor(bSaveActive ? TabInactiveTextColor : TabActiveTextColor));
	}
}

void UBonfireMenuWidget::HandleTabSaveClicked()
{
	SetActiveTab(true);
	QueueThumbnailRefresh(3);
}

void UBonfireMenuWidget::HandleTabCraftClicked()
{
	SetActiveTab(false);
}
