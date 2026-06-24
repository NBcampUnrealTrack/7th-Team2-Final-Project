#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RetrieveQuestTrackerWidget.generated.h"

class UQuestTrackerViewModel;

UCLASS()
class RETRIEVE_API URetrieveQuestTrackerWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
};
