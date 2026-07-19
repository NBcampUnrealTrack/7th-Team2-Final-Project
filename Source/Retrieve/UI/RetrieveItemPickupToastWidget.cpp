#include "UI/RetrieveItemPickupToastWidget.h"

#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "TimerManager.h"

void URetrieveItemPickupToastWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!ToastVFXTarget)
	{
		ToastVFXTarget = GetWidgetFromName(TEXT("ToastBG"));
	}
	if (!DefaultVFXTarget && ToastVFXTarget)
	{
		DefaultVFXTarget = ToastVFXTarget;
	}

	BoundAnim_In = FindAnimationByName(TEXT("Anim_In"));
	BoundAnim_Out = FindAnimationByName(TEXT("Anim_Out"));

	UE_LOG(LogTemp, Log,
		TEXT("[ToastWidget] Animation binding: Anim_In=%s Anim_Out=%s Class=%s"),
		*GetNameSafe(BoundAnim_In), *GetNameSafe(BoundAnim_Out), *GetClass()->GetPathName());

	if (BoundAnim_In)
	{
		PlayAnimation(BoundAnim_In);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[ToastWidget] Anim_In is not bound."));
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ExitAnimationTimerHandle,
			this,
			&URetrieveItemPickupToastWidget::PlayExitAnimation,
			2.0f,
			false);
	}
}

void URetrieveItemPickupToastWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ExitAnimationTimerHandle);
	}

	Super::NativeDestruct();
}

void URetrieveItemPickupToastWidget::PlayExitAnimation()
{
	if (BoundAnim_Out)
	{
		PlayAnimation(BoundAnim_Out);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[ToastWidget] Anim_Out is not bound."));
	}
}

UWidgetAnimation* URetrieveItemPickupToastWidget::FindAnimationByName(FName AnimationName) const
{
	const FString ExpectedObjectName = AnimationName.ToString() + TEXT("_INST");

	for (const UClass* CurrentClass = GetClass(); CurrentClass;
		CurrentClass = CurrentClass->GetSuperClass())
	{
		const UWidgetBlueprintGeneratedClass* WidgetClass =
			Cast<UWidgetBlueprintGeneratedClass>(CurrentClass);
		if (!WidgetClass)
		{
			continue;
		}

		for (UWidgetAnimation* Animation : WidgetClass->Animations)
		{
			if (Animation && (Animation->GetFName() == AnimationName
				|| Animation->GetName() == ExpectedObjectName))
			{
				return Animation;
			}
		}
	}

	return nullptr;
}

void URetrieveItemPickupToastWidget::InitToast(FName ItemId, FGameplayTag ItemCategoryTag, int32 Quantity)
{
	UE_LOG(LogTemp, Warning, TEXT("[ToastWidget] InitToast CALLED: ItemId=%s Tag=%s Qty=%d | ItemNameText=%s QuantityText=%s"),
		*ItemId.ToString(),
		*ItemCategoryTag.ToString(),
		Quantity,
		ItemNameText ? TEXT("VALID") : TEXT("NULL"),
		QuantityText ? TEXT("VALID") : TEXT("NULL"));

	const FText DisplayName = LookupItemDisplayName(ItemId, ItemCategoryTag);

	UE_LOG(LogTemp, Warning, TEXT("[ToastWidget] DisplayName='%s'"), *DisplayName.ToString());

	// 위젯 트리의 Image 이름이 'ItemIcon'이라 BindWidget 멤버(ItemIconImage)와 불일치하여
	// 자동 바인딩되지 않는다(아이콘 미표시 버그). 이름으로 직접 조회한다(프로젝트의 GetWidgetFromName 패턴).
	UImage* IconImg = ItemIcon ? ItemIcon.Get() : Cast<UImage>(GetWidgetFromName(TEXT("ItemIcon")));
	if (IconImg)
	{
		if (UDataTable* IconDT = LoadObject<UDataTable>(
			nullptr, TEXT("/Game/Retrieve/Data/Items/DT_ItemIcon.DT_ItemIcon")))
		{
			if (const FRetrieveItemIconRow* IconRow =
				IconDT->FindRow<FRetrieveItemIconRow>(ItemId, TEXT("InitToast::Icon"), false))
			{
				if (UTexture2D* Tex = IconRow->IconTexture.LoadSynchronous())
				{
					IconImg->SetBrushFromTexture(Tex, false);
					IconImg->SetRenderOpacity(1.f);
					IconImg->SetVisibility(ESlateVisibility::Visible);
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("[ToastWidget] IconTexture load failed for '%s'"), *ItemId.ToString());
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[ToastWidget] DT_ItemIcon row '%s' NOT FOUND"), *ItemId.ToString());
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[ToastWidget] DT_ItemIcon load FAILED"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[ToastWidget] ItemIcon widget NOT FOUND"));
	}

	if (ItemNameText)
	{
		ItemNameText->SetText(DisplayName);
		ApplyNameAutoFit(DisplayName);
	}

	if (QuantityText)
	{
		QuantityText->SetText(FText::FromString(FString::Printf(TEXT("×%d"), Quantity)));
	}

	PlayUIVFXOnWidget(
		RetrieveGameplayTags::UI_VFX_Icon_ItemAdded,
		ToastVFXTarget ? ToastVFXTarget.Get() : ResolveDefaultVFXTarget());
}

void URetrieveItemPickupToastWidget::InitCustomToast(const FText& Title, UTexture2D* Icon, const FText& InQuantityText)
{
	// 아이콘: BindWidget 멤버(ItemIcon)가 이름 불일치로 null일 수 있어 이름으로도 조회.
	UImage* IconImg = ItemIcon ? ItemIcon.Get() : Cast<UImage>(GetWidgetFromName(TEXT("ItemIcon")));
	if (IconImg)
	{
		if (Icon)
		{
			IconImg->SetBrushFromTexture(Icon, false);
			IconImg->SetRenderOpacity(1.f);
			IconImg->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			IconImg->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (ItemNameText)
	{
		ItemNameText->SetText(Title);
		ApplyNameAutoFit(Title);
	}

	if (QuantityText)
	{
		if (InQuantityText.IsEmptyOrWhitespace())
		{
			QuantityText->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			QuantityText->SetVisibility(ESlateVisibility::HitTestInvisible);
			QuantityText->SetText(InQuantityText);
		}
	}

	PlayUIVFXOnWidget(
		RetrieveGameplayTags::UI_VFX_Icon_ItemAdded,
		ToastVFXTarget ? ToastVFXTarget.Get() : ResolveDefaultVFXTarget());
}

void URetrieveItemPickupToastWidget::SetToastSlotIndex(int32 SlotIndex, float SlotStartY, float SlotHeight)
{
	UWidget* BG = GetWidgetFromName(TEXT("ToastBG"));
	if (!BG)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ToastWidget] SetToastSlotIndex: ToastBG not found"));
		return;
	}

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(BG->Slot))
	{
		// center_right 앵커: Y=0이 화면 수직 중앙, 음수 Y = 위쪽
		// alignment(1, 0) = 위젯 오른쪽 상단 모서리가 앵커 포인트에 맞춰짐
		CanvasSlot->SetAnchors(FAnchors(1.f, 0.5f, 1.f, 0.5f));
		CanvasSlot->SetAlignment(FVector2D(1.f, 0.f));
		CanvasSlot->SetPosition(FVector2D(-16.f, SlotStartY + SlotIndex * SlotHeight));
	}
}

void URetrieveItemPickupToastWidget::ApplyNameAutoFit(const FText& Name)
{
	if (!ItemNameText)
	{
		return;
	}

	FSlateFontInfo Font = ItemNameText->GetFont();
	if (BaseNameFontSize <= 0)
	{
		BaseNameFontSize = Font.Size; // WBP 기본 크기 캐싱(짧은 이름은 이 크기로 원복)
	}

	// 토스트 이름 영역의 가용 폭(px). ToastBG 300 - 아이콘/여백 ≈ 200.
	// 한글 글자폭 ≈ 폰트 크기 1.0배, 라틴·숫자·공백 ≈ 0.55배로 가중 길이를 추정한다.
	float WeightedLen = 0.f;
	for (const TCHAR Ch : Name.ToString())
	{
		WeightedLen += (Ch <= 0x2FF) ? 0.55f : 1.f;
	}

	constexpr float AvailablePx = 200.f;
	constexpr int32 MinFontSize = 11;

	int32 NewSize = BaseNameFontSize;
	if (WeightedLen > 0.f && WeightedLen * BaseNameFontSize > AvailablePx)
	{
		NewSize = FMath::Clamp(
			FMath::FloorToInt(AvailablePx / WeightedLen), MinFontSize, BaseNameFontSize);
	}

	Font.Size = NewSize;
	ItemNameText->SetFont(Font);
}

FText URetrieveItemPickupToastWidget::LookupItemDisplayName(FName ItemId, FGameplayTag ItemCategoryTag)
{
	// 각 카테고리 테이블에서 이름 조회(못 찾거나 DisplayName이 비면 빈 텍스트 반환).
	// LoadObject에 전체 경로(패키지.오브젝트) 사용 — 경로만 쓰면 찾지 못할 수 있음.
	auto FromWeapon = [ItemId]() -> FText
	{
		if (const UDataTable* DT = LoadObject<UDataTable>(nullptr, TEXT("/Game/Retrieve/Data/Items/DT_WeaponData.DT_WeaponData")))
			if (const FRetrieveWeaponDataRow* Row = DT->FindRow<FRetrieveWeaponDataRow>(ItemId, TEXT("InitToast"), false))
				if (!Row->DisplayName.IsEmptyOrWhitespace()) return Row->DisplayName;
		return FText::GetEmpty();
	};
	auto FromArmor = [ItemId]() -> FText
	{
		if (const UDataTable* DT = LoadObject<UDataTable>(nullptr, TEXT("/Game/Retrieve/Data/Items/DT_Armor.DT_Armor")))
			if (const FRetrieveArmorDataRow* Row = DT->FindRow<FRetrieveArmorDataRow>(ItemId, TEXT("InitToast"), false))
				if (!Row->DisplayName.IsEmptyOrWhitespace()) return Row->DisplayName;
		return FText::GetEmpty();
	};
	auto FromConsumable = [ItemId]() -> FText
	{
		if (const UDataTable* DT = LoadObject<UDataTable>(nullptr, TEXT("/Game/Retrieve/Data/Items/DT_ConsumableItem.DT_ConsumableItem")))
			if (const FRetrieveConsumableItemRow* Row = DT->FindRow<FRetrieveConsumableItemRow>(ItemId, TEXT("InitToast"), false))
				if (!Row->DisplayName.IsEmptyOrWhitespace()) return Row->DisplayName;
		return FText::GetEmpty();
	};
	auto FromMaterial = [ItemId]() -> FText
	{
		if (const UDataTable* DT = LoadObject<UDataTable>(nullptr, TEXT("/Game/Retrieve/Data/Items/DT_MaterialItem.DT_MaterialItem")))
			if (const FRetrieveMaterialItemRow* Row = DT->FindRow<FRetrieveMaterialItemRow>(ItemId, TEXT("InitToast"), false))
				if (!Row->DisplayName.IsEmptyOrWhitespace()) return Row->DisplayName;
		return FText::GetEmpty();
	};

	// 1) 카테고리 태그가 가리키는 테이블 우선 조회.
	FText Name;
	if (ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Weapon))          Name = FromWeapon();
	else if (ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Armor))      Name = FromArmor();
	else if (ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Consumable)) Name = FromConsumable();
	else if (ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Material))   Name = FromMaterial();
	if (!Name.IsEmpty()) return Name;

	// 2) 카테고리가 없거나(None) 해당 테이블에 없으면 모든 테이블을 순회(카테고리 오지정/누락 보정).
	Name = FromWeapon();     if (!Name.IsEmpty()) return Name;
	Name = FromArmor();      if (!Name.IsEmpty()) return Name;
	Name = FromConsumable(); if (!Name.IsEmpty()) return Name;
	Name = FromMaterial();   if (!Name.IsEmpty()) return Name;

	// 3) 어디에도 이름이 없으면 최후에 ID를 표시.
	UE_LOG(LogTemp, Warning, TEXT("[ToastWidget] LookupItemDisplayName: no DisplayName for '%s' (tag=%s)"),
		*ItemId.ToString(), *ItemCategoryTag.ToString());
	return FText::FromName(ItemId);
}
