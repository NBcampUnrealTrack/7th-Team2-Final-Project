#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UI/VFX/RetrieveUIVFXWidget.h"
#include "RetrieveItemAddedVFXWidget.generated.h"

class UInventoryComponent;
class UWidget;

UCLASS(Abstract, Blueprintable)
class RETRIEVE_API URetrieveItemAddedVFXWidget : public URetrieveUIVFXWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|UI VFX|Item")
	TObjectPtr<UWidget> ItemIconVFXTarget;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|UI VFX|Item")
	TObjectPtr<UInventoryComponent> BoundInventoryComponent;

	UFUNCTION(BlueprintImplementableEvent, Category = "Retrieve|UI VFX|Item")
	void BP_OnItemAdded(FName ItemId, FGameplayTag ItemCategoryTag, int32 Quantity);

	UFUNCTION()
	void HandleItemAdded(FName ItemId, FGameplayTag ItemCategoryTag, int32 Quantity);

	UInventoryComponent* ResolveInventoryComponent() const;
	UWidget* ResolveItemIconVFXTarget() const;
};
