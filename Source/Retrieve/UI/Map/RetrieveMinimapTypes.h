#pragma once

#include "CoreMinimal.h"
#include "RetrieveMinimapTypes.generated.h"

class ARetrieveMinimapAreaVolume;
class UTexture;

UENUM(BlueprintType)
enum class ERetrieveMinimapDisplayMode : uint8
{
	WorldMap        UMETA(DisplayName="World Map"),
	Black           UMETA(DisplayName="Black"),
	BakedTexture    UMETA(DisplayName="Baked Texture"),
	LiveRenderTarget UMETA(DisplayName="Live Render Target"),
};

/**
 * A resolved minimap coordinate space. The outdoor context is built from
 * DA_MapConfig; indoor contexts are built by ARetrieveMinimapAreaVolume.
 */
USTRUCT(BlueprintType)
struct FRetrieveMinimapContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Retrieve|Minimap")
	ERetrieveMinimapDisplayMode DisplayMode = ERetrieveMinimapDisplayMode::WorldMap;

	UPROPERTY(BlueprintReadOnly, Category="Retrieve|Minimap")
	TObjectPtr<UTexture> Texture = nullptr;

	/** Center of the captured square in world XY. */
	UPROPERTY(BlueprintReadOnly, Category="Retrieve|Minimap")
	FVector2D MapCenter = FVector2D::ZeroVector;

	/** Full captured width along World X/Y. */
	UPROPERTY(BlueprintReadOnly, Category="Retrieve|Minimap")
	FVector2D MapExtentXY = FVector2D(1.0f, 1.0f);

	UPROPERTY(BlueprintReadOnly, Category="Retrieve|Minimap")
	float ViewWorldRadius = 3000.0f;

	UPROPERTY(BlueprintReadOnly, Category="Retrieve|Minimap")
	float MinZ = -BIG_NUMBER;

	UPROPERTY(BlueprintReadOnly, Category="Retrieve|Minimap")
	float MaxZ = BIG_NUMBER;

	UPROPERTY(BlueprintReadOnly, Category="Retrieve|Minimap")
	bool bShowIcons = true;

	UPROPERTY(BlueprintReadOnly, Category="Retrieve|Minimap")
	bool bShowWaypoints = true;

	UPROPERTY(BlueprintReadOnly, Category="Retrieve|Minimap")
	bool bShowPlayerMarker = true;

	UPROPERTY(BlueprintReadOnly, Category="Retrieve|Minimap")
	TObjectPtr<ARetrieveMinimapAreaVolume> SourceArea = nullptr;

	bool IsIndoor() const { return SourceArea != nullptr; }
	bool HasDrawableTexture() const { return Texture != nullptr; }

	FVector2D WorldToUV(const FVector& WorldLocation) const
	{
		const float SafeExtentX = FMath::Max(MapExtentXY.X, 1.0f);
		const float SafeExtentY = FMath::Max(MapExtentXY.Y, 1.0f);
		const float U = (WorldLocation.Y - MapCenter.Y) / SafeExtentY + 0.5f;
		const float V = 0.5f - (WorldLocation.X - MapCenter.X) / SafeExtentX;
		return FVector2D(U, V);
	}

	float GetZoom() const
	{
		const float SafeRadius = FMath::Max(ViewWorldRadius, 1.0f);
		const float ShortExtent = FMath::Min(
			FMath::Max(MapExtentXY.X, 1.0f),
			FMath::Max(MapExtentXY.Y, 1.0f));
		return ShortExtent / (SafeRadius * 2.0f);
	}

	bool ContainsZ(float Z) const
	{
		return Z >= MinZ && Z <= MaxZ;
	}
};

