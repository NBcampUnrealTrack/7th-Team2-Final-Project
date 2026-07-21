#include "UI/HUD/RetrieveQuickSlotWheelWidget.h"

#include "Components/Button.h"
#include "Components/Inventory/InventoryComponent.h"
#include "InputCoreTypes.h"
#include "Input/Reply.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "UObject/UnrealType.h"

void URetrieveQuickSlotWheelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindSlotButtons();

	if (Button_CloseWheel)
	{
		Button_CloseWheel->OnClicked.RemoveDynamic(this, &ThisClass::HandleCloseWheelClicked);
		Button_CloseWheel->OnClicked.AddDynamic(this, &ThisClass::HandleCloseWheelClicked);
		// 기본은 숨김 — Assign 모드로 열릴 때만 표시한다.
		Button_CloseWheel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void URetrieveQuickSlotWheelWidget::HandleCloseWheelClicked()
{
	CloseWheel();
}

void URetrieveQuickSlotWheelWidget::BindSlotButtons()
{
	if (bSlotButtonsBound)
	{
		return;
	}

	bool bAnyBound = false;

#define BIND_SLOT_BUTTON(SlotWidget, HandlerName) \
	if (UButton* Btn = GetSlotButton(SlotWidget)) \
	{ \
		Btn->OnClicked.RemoveDynamic(this, &ThisClass::HandlerName); \
		Btn->OnClicked.AddDynamic(this, &ThisClass::HandlerName); \
		bAnyBound = true; \
	}

	BIND_SLOT_BUTTON(Item_00, HandleItem00Clicked);
	BIND_SLOT_BUTTON(Item_01, HandleItem01Clicked);
	BIND_SLOT_BUTTON(Item_02, HandleItem02Clicked);
	BIND_SLOT_BUTTON(Item_03, HandleItem03Clicked);
	BIND_SLOT_BUTTON(Item_04, HandleItem04Clicked);
	BIND_SLOT_BUTTON(Item_05, HandleItem05Clicked);

#undef BIND_SLOT_BUTTON

	bSlotButtonsBound = bAnyBound;
}

UButton* URetrieveQuickSlotWheelWidget::GetSlotButton(UUserWidget* SlotWidget) const
{
	if (!SlotWidget)
	{
		return nullptr;
	}

	// 슬롯 위젯(Button_FantasyWarrior_WeaponWheel_Item_01_C) 내부의 전체 영역 버튼
	return Cast<UButton>(SlotWidget->GetWidgetFromName(TEXT("BUTTON")));
}

void URetrieveQuickSlotWheelWidget::InitializeQuickSlotWheel(
	UInventoryComponent* InInventoryComponent,
	UDataTable* InItemIconTable)
{
	InventoryComponent = InInventoryComponent;
	ItemIconTable = InItemIconTable;

	if (InventoryComponent)
	{
		InventoryComponent->OnQuickSlotChanged.RemoveDynamic(
			this,
			&ThisClass::HandleQuickSlotChanged);

		InventoryComponent->OnQuickSlotChanged.AddDynamic(
			this,
			&ThisClass::HandleQuickSlotChanged);
	}

	RefreshFromQuickSlots();
}

void URetrieveQuickSlotWheelWidget::NativeDestruct()
{
	if (InventoryComponent)
	{
		InventoryComponent->OnQuickSlotChanged.RemoveDynamic(
			this,
			&ThisClass::HandleQuickSlotChanged);
	}

	Super::NativeDestruct();
}

void URetrieveQuickSlotWheelWidget::HandleQuickSlotChanged(int32 SlotKey, FRetrieveQuickSlotEntry Entry)
{
	if (UUserWidget* SlotWidget = GetSlotWidget(SlotKey))
	{
		if (Entry.IsValid())
		{
			ApplyItemToWheelSlot(SlotWidget, Entry);
		}
		else
		{
			ClearWheelSlot(SlotWidget);
		}
	}
}

void URetrieveQuickSlotWheelWidget::RefreshFromQuickSlots()
{
	if (!InventoryComponent)
	{
		for (int32 SlotIndex = 0; SlotIndex <= 5; ++SlotIndex)
		{
			ClearWheelSlot(GetSlotWidget(SlotIndex));
		}
		return;
	}

	for (int32 SlotIndex = 0; SlotIndex <= 5; ++SlotIndex)
	{
		UUserWidget* SlotWidget = GetSlotWidget(SlotIndex);
		if (!SlotWidget)
		{
			continue;
		}

		const FRetrieveQuickSlotEntry Entry = InventoryComponent->GetQuickSlotEntry(SlotIndex);
		if (Entry.IsValid())
		{
			ApplyItemToWheelSlot(SlotWidget, Entry);
		}
		else
		{
			ClearWheelSlot(SlotWidget);
		}
	}
}

void URetrieveQuickSlotWheelWidget::OpenForAssign(FName InPendingItemId, FGameplayTag InPendingCategoryTag)
{
	WheelMode = EQuickSlotWheelMode::Assign;
	PendingAssignItemId = InPendingItemId;
	PendingAssignCategoryTag = InPendingCategoryTag;

	// 인벤토리에서 열릴 때는 약 1/3 크기로 축소해서 표시
	constexpr float AssignModeScale = 0.34f;
	SetRenderScale(FVector2D(AssignModeScale, AssignModeScale));

	// 인벤토리 패널(우측)과 겹치지 않도록 휠을 화면 좌측으로 이동시킨다.
	float OffsetX = 520.f;
	if (APlayerController* PC = GetOwningPlayer())
	{
		int32 ViewportX = 0;
		int32 ViewportY = 0;
		PC->GetViewportSize(ViewportX, ViewportY);
		if (ViewportX > 0)
		{
			OffsetX = ViewportX * 0.27f;
		}
	}
	SetRenderTranslation(FVector2D(-OffsetX, 0.f));

	// Assign(인벤토리 등록) 모드에서만 닫기 버튼을 보여준다.
	if (Button_CloseWheel)
	{
		Button_CloseWheel->SetVisibility(ESlateVisibility::Visible);
	}

	SetVisibility(ESlateVisibility::Visible);
	RefreshFromQuickSlots();

	// ESC로 닫을 수 있도록 키보드 포커스 부여
	SetIsFocusable(true);
	SetKeyboardFocus();
}

void URetrieveQuickSlotWheelWidget::OpenForUse()
{
	WheelMode = EQuickSlotWheelMode::Use;
	PendingAssignItemId = NAME_None;
	PendingAssignCategoryTag = FGameplayTag();
	HighlightedSlotKey = INDEX_NONE;

	// 인게임 사용 모드도 화면에 비해 너무 크므로 축소해서 표시
	constexpr float UseModeScale = 0.45f;
	SetRenderScale(FVector2D(UseModeScale, UseModeScale));
	// 인게임 사용 모드는 화면 중앙에 표시한다.
	SetRenderTranslation(FVector2D::ZeroVector);

	// 인게임 사용 모드에서는 닫기 버튼을 숨긴다.
	if (Button_CloseWheel)
	{
		Button_CloseWheel->SetVisibility(ESlateVisibility::Collapsed);
	}

	// 사용 모드에서는 입력(휠 키 누름/뗌)을 EnhancedInput(IA_QuickSlotWheel)→컨트롤러가 처리하므로
	// 휠이 키보드 포커스를 가로채지 않도록 한다. (마우스 방향 선택은 NativeTick으로 동작)
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	RefreshFromQuickSlots();
}

void URetrieveQuickSlotWheelWidget::CloseWheel()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

FReply URetrieveQuickSlotWheelWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	// ESC 또는 게임패드 B로 휠 닫기 (등록 취소)
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
	{
		CloseWheel();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void URetrieveQuickSlotWheelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const ESlateVisibility Vis = GetVisibility();
	const bool bOpen = (Vis != ESlateVisibility::Collapsed && Vis != ESlateVisibility::Hidden);

	// 인게임 사용 모드로 열려 있을 때만 마우스 방향으로 슬롯을 하이라이트
	if (WheelMode == EQuickSlotWheelMode::Use && bOpen)
	{
		UpdateRadialHighlight(MyGeometry);
	}
}

int32 URetrieveQuickSlotWheelWidget::ComputeSlotFromMouse(const FGeometry& MyGeometry) const
{
	if (!FSlateApplication::IsInitialized())
	{
		return INDEX_NONE;
	}

	const FVector2D AbsMouse = FSlateApplication::Get().GetCursorPos();
	const FVector2D Local = MyGeometry.AbsoluteToLocal(AbsMouse);
	const FVector2D Center = MyGeometry.GetLocalSize() * 0.5f;
	const FVector2D Dir = Local - Center;

	// 중앙 데드존: 너무 가까우면 선택 없음
	const float DeadZone = FMath::Min(Center.X, Center.Y) * 0.2f;
	if (Dir.SizeSquared() < DeadZone * DeadZone)
	{
		return INDEX_NONE;
	}

	// 화면 좌표(Y 아래 방향) 기준 각도. 위쪽(Item_00)을 0도로, 시계방향으로 60도씩 6슬롯.
	const float AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
	float Relative = AngleDeg + 90.0f;            // 위쪽 기준
	Relative = FMath::Fmod(Relative + 360.0f, 360.0f);

	const int32 SlotIndex = FMath::RoundToInt(Relative / 60.0f) % 6;
	return SlotIndex;
}

void URetrieveQuickSlotWheelWidget::UpdateRadialHighlight(const FGeometry& MyGeometry)
{
	const int32 NewSlot = ComputeSlotFromMouse(MyGeometry);
	if (NewSlot == HighlightedSlotKey)
	{
		return;
	}

	SetSlotHighlighted(HighlightedSlotKey, false);
	HighlightedSlotKey = NewSlot;
	SetSlotHighlighted(HighlightedSlotKey, true);
}

void URetrieveQuickSlotWheelWidget::SetSlotHighlighted(int32 SlotKey, bool bHighlighted)
{
	if (SlotKey == INDEX_NONE)
	{
		return;
	}

	if (UUserWidget* SlotWidget = GetSlotWidget(SlotKey))
	{
		SetBoolProperty(SlotWidget, TEXT("bSelected"), bHighlighted);
		CallSlotRefreshFunction(SlotWidget);
	}
}

void URetrieveQuickSlotWheelWidget::ActivateHighlightedSlotAndClose()
{
	if (WheelMode == EQuickSlotWheelMode::Use
		&& HighlightedSlotKey != INDEX_NONE
		&& InventoryComponent)
	{
		InventoryComponent->ActivateQuickSlotItem(HighlightedSlotKey);
	}

	SetSlotHighlighted(HighlightedSlotKey, false);
	HighlightedSlotKey = INDEX_NONE;
	CloseWheel();
}

void URetrieveQuickSlotWheelWidget::HandleItem00Clicked() { HandleWheelSlotClicked(0); }
void URetrieveQuickSlotWheelWidget::HandleItem01Clicked() { HandleWheelSlotClicked(1); }
void URetrieveQuickSlotWheelWidget::HandleItem02Clicked() { HandleWheelSlotClicked(2); }
void URetrieveQuickSlotWheelWidget::HandleItem03Clicked() { HandleWheelSlotClicked(3); }
void URetrieveQuickSlotWheelWidget::HandleItem04Clicked() { HandleWheelSlotClicked(4); }
void URetrieveQuickSlotWheelWidget::HandleItem05Clicked() { HandleWheelSlotClicked(5); }

void URetrieveQuickSlotWheelWidget::HandleWheelSlotClicked(int32 SlotKey)
{
	if (WheelMode == EQuickSlotWheelMode::Use)
	{
		if (InventoryComponent)
		{
			InventoryComponent->ActivateQuickSlotItem(SlotKey);
		}
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// Assign 모드: 상위 위젯(InventoryPanelWidget)이 처리하도록 델리게이트 전달
	OnWheelSlotClicked.Broadcast(SlotKey);
}

UUserWidget* URetrieveQuickSlotWheelWidget::GetSlotWidget(int32 SlotKey) const
{
	switch (SlotKey)
	{
	case 0: return Item_00;
	case 1: return Item_01;
	case 2: return Item_02;
	case 3: return Item_03;
	case 4: return Item_04;
	case 5: return Item_05;
	default: return nullptr;
	}
}

void URetrieveQuickSlotWheelWidget::ApplyItemToWheelSlot(
	UUserWidget* SlotWidget,
	const FRetrieveQuickSlotEntry& Entry)
{
	if (!SlotWidget || !InventoryComponent || !Entry.IsValid())
	{
		ClearWheelSlot(SlotWidget);
		return;
	}

	UTexture2D* IconTexture = nullptr;

	if (ItemIconTable)
	{
		if (const FRetrieveItemIconRow* IconRow =
			ItemIconTable->FindRow<FRetrieveItemIconRow>(
				Entry.ItemId,
				TEXT("QuickSlotWheel::ApplyItemToWheelSlot")))
		{
			IconTexture = IconRow->IconTexture.LoadSynchronous();
		}
	}

	const bool bIsConsumable =
		Entry.ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Consumable);

	const int32 Count = bIsConsumable
		? InventoryComponent->GetItemCount(Entry.ItemId)
		: 0;

	SetObjectProperty(SlotWidget, TEXT("Texture_ItemIcon"), IconTexture);
	SetBoolProperty(SlotWidget, TEXT("bUseIconMask"), true);
	SetLinearColorProperty(SlotWidget, TEXT("Color_ItemIcon"), FLinearColor::White);
	SetVector2DProperty(SlotWidget, TEXT("Icon_Scale"), FVector2D(1.0f, 1.0f));
	SetMarginProperty(SlotWidget, TEXT("Icon_Offsets"), FMargin(0.0f, 35.0f, 0.0f, 0.0f));
	SetBoolProperty(SlotWidget, TEXT("bSelected"), false);
	SetBoolProperty(SlotWidget, TEXT("bDeslectSiblings"), true);
	SetBoolProperty(SlotWidget, TEXT("bShowConsumables"), bIsConsumable);
	SetIntProperty(SlotWidget, TEXT("Consumable_Count"), Count);
	SetBoolProperty(SlotWidget, TEXT("bDemoActive"), false);

	// 아이콘 자식 위젯을 명시적으로 표시 (비울 때 숨겨두므로 다시 켜준다)
	if (UWidget* IconWidget = SlotWidget->GetWidgetFromName(TEXT("ICON")))
	{
		IconWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	CallSlotRefreshFunction(SlotWidget);
}

void URetrieveQuickSlotWheelWidget::ClearWheelSlot(UUserWidget* SlotWidget)
{
	if (!SlotWidget)
	{
		return;
	}

	SetObjectProperty(SlotWidget, TEXT("Texture_ItemIcon"), nullptr);
	SetBoolProperty(SlotWidget, TEXT("bUseIconMask"), true);
	SetLinearColorProperty(SlotWidget, TEXT("Color_ItemIcon"), FLinearColor(1.f, 1.f, 1.f, 0.f));
	SetBoolProperty(SlotWidget, TEXT("bSelected"), false);
	SetBoolProperty(SlotWidget, TEXT("bShowConsumables"), false);
	SetIntProperty(SlotWidget, TEXT("Consumable_Count"), 0);
	SetBoolProperty(SlotWidget, TEXT("bDemoActive"), false);

	// PreConstruct가 null 텍스처로 아이콘을 지우지 않으므로 아이콘 자식을 직접 숨겨 확실히 비운다
	if (UWidget* IconWidget = SlotWidget->GetWidgetFromName(TEXT("ICON")))
	{
		IconWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	CallSlotRefreshFunction(SlotWidget);
}

void URetrieveQuickSlotWheelWidget::SetObjectProperty(
	UUserWidget* TargetWidget,
	FName PropertyName,
	UObject* Value)
{
	if (!TargetWidget) return;

	if (FObjectProperty* Property =
		FindFProperty<FObjectProperty>(TargetWidget->GetClass(), PropertyName))
	{
		Property->SetObjectPropertyValue_InContainer(TargetWidget, Value);
	}
}

void URetrieveQuickSlotWheelWidget::SetBoolProperty(
	UUserWidget* TargetWidget,
	FName PropertyName,
	bool bValue)
{
	if (!TargetWidget) return;

	if (FBoolProperty* Property =
		FindFProperty<FBoolProperty>(TargetWidget->GetClass(), PropertyName))
	{
		Property->SetPropertyValue_InContainer(TargetWidget, bValue);
	}
}

void URetrieveQuickSlotWheelWidget::SetIntProperty(
	UUserWidget* TargetWidget,
	FName PropertyName,
	int32 Value)
{
	if (!TargetWidget) return;

	if (FIntProperty* Property =
		FindFProperty<FIntProperty>(TargetWidget->GetClass(), PropertyName))
	{
		Property->SetPropertyValue_InContainer(TargetWidget, Value);
	}
}

void URetrieveQuickSlotWheelWidget::SetLinearColorProperty(
	UUserWidget* TargetWidget,
	FName PropertyName,
	const FLinearColor& Value)
{
	if (!TargetWidget) return;

	if (FStructProperty* Property =
		FindFProperty<FStructProperty>(TargetWidget->GetClass(), PropertyName))
	{
		if (Property->Struct == TBaseStructure<FLinearColor>::Get())
		{
			*Property->ContainerPtrToValuePtr<FLinearColor>(TargetWidget) = Value;
		}
	}
}

void URetrieveQuickSlotWheelWidget::SetVector2DProperty(
	UUserWidget* TargetWidget,
	FName PropertyName,
	const FVector2D& Value)
{
	if (!TargetWidget) return;

	if (FStructProperty* Property =
		FindFProperty<FStructProperty>(TargetWidget->GetClass(), PropertyName))
	{
		if (Property->Struct == TBaseStructure<FVector2D>::Get())
		{
			*Property->ContainerPtrToValuePtr<FVector2D>(TargetWidget) = Value;
		}
	}
}

void URetrieveQuickSlotWheelWidget::SetMarginProperty(
	UUserWidget* TargetWidget,
	FName PropertyName,
	const FMargin& Value)
{
	if (!TargetWidget) return;

	if (FStructProperty* Property =
		FindFProperty<FStructProperty>(TargetWidget->GetClass(), PropertyName))
	{
		if (Property->Struct == TBaseStructure<FMargin>::Get())
		{
			*Property->ContainerPtrToValuePtr<FMargin>(TargetWidget) = Value;
		}
	}
}

void URetrieveQuickSlotWheelWidget::CallSlotRefreshFunction(UUserWidget* SlotWidget)
{
	if (!SlotWidget) return;

	// 커스텀 갱신 함수 우선, 없으면 프로퍼티 기반 비주얼을 다시 그리는 PreConstruct
	// (Tick은 파라미터가 복잡하고 수동 호출이 위험하므로 제외)
	static const FName RefreshNames[] =
	{
		TEXT("RefreshItemVisual"),
		TEXT("Update"),
		TEXT("Refresh"),
		TEXT("UpdateVisual"),
		TEXT("PreConstruct")
	};

	for (const FName FunctionName : RefreshNames)
	{
		UFunction* Function = SlotWidget->FindFunction(FunctionName);
		if (!Function)
		{
			continue;
		}

		if (Function->ParmsSize > 0)
		{
			// PreConstruct(IsDesignTime) 등 파라미터가 있는 함수는
			// 0으로 초기화된 버퍼를 넘겨 null 역참조 크래시를 방지
			void* Params = FMemory_Alloca(Function->ParmsSize);
			FMemory::Memzero(Params, Function->ParmsSize);
			SlotWidget->ProcessEvent(Function, Params);
		}
		else
		{
			SlotWidget->ProcessEvent(Function, nullptr);
		}
		return;
	}
}
