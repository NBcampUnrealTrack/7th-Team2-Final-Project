#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UI/Map/RetrieveMinimapTypes.h"
#include "RetrieveMinimapAreaDataAsset.generated.h"

class UTexture2D;

/** Visual and filtering policy shared by one or more indoor minimap areas. */
UCLASS(BlueprintType)
class RETRIEVE_API URetrieveMinimapAreaDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Retrieve|Minimap")
	ERetrieveMinimapDisplayMode DisplayMode = ERetrieveMinimapDisplayMode::Black;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Retrieve|Minimap",
		meta=(EditCondition="DisplayMode==ERetrieveMinimapDisplayMode::BakedTexture", EditConditionHides))
	TObjectPtr<UTexture2D> BakedTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Retrieve|Minimap", meta=(ClampMin="100.0"))
	float ViewWorldRadius = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Retrieve|Minimap")
	bool bShowIcons = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Retrieve|Minimap")
	bool bShowWaypoints = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Retrieve|Minimap")
	bool bShowPlayerMarker = true;
};

