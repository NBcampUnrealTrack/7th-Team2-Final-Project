#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RetrieveHotBarWidget.generated.h"

class URetrieveHealthComponent;
class UStaminaComponent;

/**
 * Combined player HP / stamina HUD.
 *
 * WBP_HotBar already owns the two Fantasy Warrior bar widgets. This class
 * connects those visual bars to the local pawn's health and stamina
 * components while keeping the Blueprint layout fully editable.
 */
UCLASS()
class RETRIEVE_API URetrieveHotBarWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> HealthBarWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> StaminaBarWidget;

	UPROPERTY(Transient)
	TObjectPtr<URetrieveHealthComponent> HealthComponent;

	UPROPERTY(Transient)
	TObjectPtr<UStaminaComponent> StaminaComponent;

	TWeakObjectPtr<APawn> BoundPawn;

	void TryBindToOwningPawn();
	void UnbindFromComponents();
	void RefreshAllBars();
	void RefreshHealthBar();
	void RefreshStaminaBar();

	UFUNCTION()
	void HandleHealthChanged(float NewHealth);

	UFUNCTION()
	void HandleMaxHealthChanged(float NewMaxHealth);

	UFUNCTION()
	void HandleStaminaChanged(float NewStamina, float MaxStamina);

	static void UpdateFantasyBar(UUserWidget* BarWidget, float CurrentValue, float MaxValue);
	static bool SetIntegerProperty(UObject* Object, FName PropertyName, int32 Value);
	static bool CallNoArgFunction(UObject* Object, FName FunctionName);
};
