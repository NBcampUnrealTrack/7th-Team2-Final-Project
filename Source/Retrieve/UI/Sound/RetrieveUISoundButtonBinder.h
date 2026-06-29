#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RetrieveUISoundButtonBinder.generated.h"

class UButton;
class URetrieveUIVFXWidget;

UCLASS()
class RETRIEVE_API URetrieveUISoundButtonBinder : public UObject
{
	GENERATED_BODY()

public:
	void Bind(URetrieveUIVFXWidget* InOwner, UButton* InButton);
	void Unbind();
	bool IsBoundTo(const UButton* Button) const;

private:
	UFUNCTION()
	void HandleHovered();

	UFUNCTION()
	void HandleUnhovered();

	UFUNCTION()
	void HandlePressed();

	UFUNCTION()
	void HandleReleased();

	UPROPERTY()
	TWeakObjectPtr<URetrieveUIVFXWidget> OwnerWidget;

	UPROPERTY()
	TWeakObjectPtr<UButton> BoundButton;
};
