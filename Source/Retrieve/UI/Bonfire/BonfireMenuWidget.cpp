#include "UI/Bonfire/BonfireMenuWidget.h"

#include "Components/Inventory/InventoryComponent.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/Pawn.h"
#include "UI/Craft/CraftPanelWidget.h"

void UBonfireMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

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
}
