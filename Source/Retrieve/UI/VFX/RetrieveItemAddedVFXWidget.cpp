#include "UI/VFX/RetrieveItemAddedVFXWidget.h"

#include "Components/InventoryComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTags/RetrieveGameplayTags.h"

void URetrieveItemAddedVFXWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BoundInventoryComponent = ResolveInventoryComponent();
	if (BoundInventoryComponent)
	{
		BoundInventoryComponent->OnItemAdded.RemoveDynamic(this, &ThisClass::HandleItemAdded);
		BoundInventoryComponent->OnItemAdded.AddDynamic(this, &ThisClass::HandleItemAdded);
	}
}

void URetrieveItemAddedVFXWidget::NativeDestruct()
{
	if (BoundInventoryComponent)
	{
		BoundInventoryComponent->OnItemAdded.RemoveDynamic(this, &ThisClass::HandleItemAdded);
		BoundInventoryComponent = nullptr;
	}

	Super::NativeDestruct();
}

void URetrieveItemAddedVFXWidget::HandleItemAdded(FName ItemId, int32 Quantity)
{
	PlayUIVFXOnWidget(RetrieveGameplayTags::UI_VFX_Icon_ItemAdded, ResolveItemIconVFXTarget());
	BP_OnItemAdded(ItemId, Quantity);
}

UInventoryComponent* URetrieveItemAddedVFXWidget::ResolveInventoryComponent() const
{
	const APlayerController* PC = GetOwningPlayer();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	return Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
}

UWidget* URetrieveItemAddedVFXWidget::ResolveItemIconVFXTarget() const
{
	if (ItemIconVFXTarget)
	{
		return ItemIconVFXTarget;
	}

	return ResolveDefaultVFXTarget();
}
