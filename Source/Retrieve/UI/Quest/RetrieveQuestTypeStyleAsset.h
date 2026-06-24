#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/RetrieveDataTableTypes.h"
#include "RetrieveQuestTypeStyleAsset.generated.h"

USTRUCT(BlueprintType)
struct FRetrieveQuestTypeStyle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FSlateBrush Frame;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FSlateBrush Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FLinearColor TextColor = FLinearColor::White;
};

UCLASS()
class RETRIEVE_API URetrieveQuestTypeStyleAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	TMap<EQuestType, FRetrieveQuestTypeStyle> Styles;

	UFUNCTION(BlueprintPure, Category = "Quest")
	bool GetStyle(EQuestType Type, FRetrieveQuestTypeStyle& OutStyle) const
	{
		if (const FRetrieveQuestTypeStyle* Found = Styles.Find(Type))
		{
			OutStyle = *Found;
			return true;
		}
		return false;
	}
};
