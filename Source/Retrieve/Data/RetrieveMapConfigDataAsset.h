#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RetrieveMapConfigDataAsset.generated.h"

class UTexture2D;
class URetrieveMapIconDataAsset;
class URetrieveMapIconRegistry;

UCLASS(BlueprintType)
class RETRIEVE_API URetrieveMapConfigDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Map")
	TObjectPtr<UTexture2D> BakedMapTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Map")
	FVector2D MapOrigin = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Map")
	FVector2D MapExtentXY = FVector2D(1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Map")
	TObjectPtr<URetrieveMapIconDataAsset> WorldMapIconData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Map")
	TObjectPtr<URetrieveMapIconRegistry> IconRegistry;
};