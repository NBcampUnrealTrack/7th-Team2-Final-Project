#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "NormalMonsterHealthBarComponent.generated.h"

class URetrieveHealthComponent;

UCLASS(ClassGroup = (Retrieve), meta = (BlueprintSpawnableComponent))
class RETRIEVE_API UNormalMonsterHealthBarComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UNormalMonsterHealthBarComponent();

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Monster HP Bar")
	void SetHealthBarEnabled(bool bNewEnabled);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION()
	void HandleHealthChanged(float NewHealth);

	UFUNCTION()
	void HandleMaxHealthChanged(float NewMaxHealth);

	UFUNCTION()
	void HandleDeathStarted(AActor* OwningActor);

private:
	void BindToHealthComponent();
	void UnbindFromHealthComponent();
	void RefreshHealthPercent();
	void ShowForDuration();
	void HideBar();
	void SetBarVisible(bool bNewVisible);
	bool ShouldShowForHealth(float Health, float MaxHealth) const;

	UPROPERTY(EditAnywhere, Category = "Retrieve|Monster HP Bar")
	bool bHealthBarEnabled = true;

	UPROPERTY(EditAnywhere, Category = "Retrieve|Monster HP Bar", meta = (ClampMin = "0.0"))
	float VisibleDurationAfterDamage = 3.f;

	UPROPERTY(EditAnywhere, Category = "Retrieve|Monster HP Bar")
	bool bHideWhenFullHealth = true;

	UPROPERTY(EditAnywhere, Category = "Retrieve|Monster HP Bar")
	bool bShowOnBeginPlayForDebug = false;

	UPROPERTY()
	TObjectPtr<URetrieveHealthComponent> BoundHealthComponent;

	FTimerHandle HideTimerHandle;
	float LastObservedHealth = -1.f;
	float LastObservedMaxHealth = -1.f;
};
