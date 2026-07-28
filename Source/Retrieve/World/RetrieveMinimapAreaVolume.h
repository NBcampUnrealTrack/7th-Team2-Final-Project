#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UI/Map/RetrieveMinimapTypes.h"
#include "RetrieveMinimapAreaVolume.generated.h"

class AActor;
class UBoxComponent;
class URetrieveMinimapAreaDataAsset;
class UTexture;
class UStaticMeshComponent;

/** Spatial minimap override for an indoor room, dungeon, cave, or floor. */
UCLASS(BlueprintType)
class RETRIEVE_API ARetrieveMinimapAreaVolume : public AActor
{
	GENERATED_BODY()

public:
	ARetrieveMinimapAreaVolume();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Retrieve|Minimap")
	TObjectPtr<UBoxComponent> AreaBounds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Retrieve|Minimap")
	TObjectPtr<URetrieveMinimapAreaDataAsset> AreaData;

	/** Higher priority wins when indoor areas overlap. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Retrieve|Minimap")
	int32 Priority = 0;

	/** Optional packed-level/root actor used by editor auto-generation. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Retrieve|Minimap")
	TObjectPtr<AActor> SourceActor;

	/** Actors carrying this tag are omitted from top-down capture (roof/ceiling). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Retrieve|Minimap|Capture")
	FName CaptureExcludeTag = TEXT("Minimap.Exclude");

	/**
	 * When SourceActor is a Packed Level Actor, capture only matching static-mesh
	 * components instead of the whole actor. This removes cave shells and roofs.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Retrieve|Minimap|Capture")
	bool bFilterSourceStaticMeshComponents = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Retrieve|Minimap|Capture",
		meta=(EditCondition="bFilterSourceStaticMeshComponents"))
	TArray<FString> CaptureIncludeMeshPatterns;

	bool ContainsLocation(const FVector& WorldLocation) const;
	FBox GetAreaBox() const;
	FRetrieveMinimapContext BuildContext() const;
	bool ShouldCaptureStaticMeshComponent(const UStaticMeshComponent* Component) const;

	UTexture* GetRuntimeTexture() const { return RuntimeTexture; }
	void SetRuntimeTexture(UTexture* InTexture) { RuntimeTexture = InTexture; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTexture> RuntimeTexture;
};
