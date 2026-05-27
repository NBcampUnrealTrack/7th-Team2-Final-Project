#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "PlayerStatusViewModel.generated.h"

class URetrieveHealthComponent;
/**
 * W_HPBar 구동. 로컬 Sovereign 폰의 URetrieveHealthComponent::OnHealthChanged / OnMaxHealthChanged에 바인딩.
 */
UCLASS(BlueprintType)
class RETRIEVE_API UPlayerStatusViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI")
	void BindToHealth(URetrieveHealthComponent* InHealth);
	
	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI")
	void UnbindFromHealth();
	
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|UI")
	float GetCurrentHealth() const { return CurrentHealth; }
	
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|UI")
	float GetMaxHealth() const { return MaxHealth; }
	
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|UI")
	float GetHealthFraction() const
	{
		return MaxHealth > 0.f ? FMath::Clamp(CurrentHealth / GetMaxHealth(), 0.f, 1.f) : 0.f;
	}
	
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|UI")
	float GetDisplayedHealthFraction() const { return DisplayedHealthFraction; }
	
	UPROPERTY(EditAnywhere, Category = "Retrieve|UI")
	float HealthInterpSpeed = 6.f;
	
private:
	UFUNCTION()
	void HandleHealthChanged(float NewHealth);
	
	UFUNCTION()
	void HandleMaxHealthChanged(float NewMaxHealth);
	
	UPROPERTY()
	TWeakObjectPtr<URetrieveHealthComponent> BoundHealth;
	
	FTSTicker::FDelegateHandle InterpTicker;
	
	float CurrentHealth = 0.f;
	float MaxHealth = 1.f;
	float DisplayedHealthFraction = 0.f;
	
	bool TickInterp(float DeltaTime);
};
