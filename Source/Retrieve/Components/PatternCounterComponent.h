#pragma once

#include "CoreMinimal.h"
#include "Components/PawnComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameplayTagContainer.h"
#include "PatternCounterComponent.generated.h"

class URetrieveAbilitySystemComponent;

UCLASS(ClassGroup = "Retrieve", meta = (BlueprintSpawnableComponent))
class RETRIEVE_API UPatternCounterComponent : public UPawnComponent
{
	GENERATED_BODY()

public:
	UPatternCounterComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void SetActivePatternRow(FName RowName, UDataTable* Table);

	void OpenCounterWindow(float WindowDuration = 0.f);

	void CloseCounterWindow();

	bool IsWindowOpen() const { return bWindowOpen; }
	
	void TryCounter(FGameplayTag ActionTag, FGameplayTag ElementTag, AActor* Instigator);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void OnWindowExpired();
	void ApplyCounterResult(AActor* Instigator);

	URetrieveAbilitySystemComponent* GetASC() const;

	bool bWindowOpen = false;

	FMonsterPatternRow ActivePatternData;
	FName ActivePatternRowName;

	FTimerHandle WindowTimerHandle;

	UPROPERTY(EditDefaultsOnly, Category = "Config", meta = (ClampMin = "0.1"))
	float DefaultWindowDuration = 0.5f;

	float GroggyCooldownExpiry = 0.f;
};
