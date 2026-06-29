#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UI/Sound/RetrieveUISoundTypes.h"
#include "RetrieveUISoundSubsystem.generated.h"

class URetrieveUIVFXWidget;
class URetrieveUISoundPreset;
class URetrieveUISoundRegistry;
class USoundBase;

UCLASS()
class RETRIEVE_API URetrieveUISoundSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category = "UI Sound")
	void SetActiveRegistry(URetrieveUISoundRegistry* NewRegistry);

	USoundBase* ResolveSound(const URetrieveUIVFXWidget* Widget, ERetrieveUISoundEvent Event) const;
	const URetrieveUISoundPreset* ResolvePreset(const URetrieveUIVFXWidget* Widget) const;

	void PlayUISound(const URetrieveUIVFXWidget* Widget, ERetrieveUISoundEvent Event) const;

private:
	UPROPERTY()
	TObjectPtr<URetrieveUISoundRegistry> ActiveRegistry;
};
