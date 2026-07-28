#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RetrieveWaterSuppressVolume.generated.h"

class UBoxComponent;

/** 이 박스 안에선 수영 진입/유지 차단. 비주얼 Water Remover의 게임플레이 대응(독립). */
UCLASS()
class RETRIEVE_API ARetrieveWaterSuppressVolume : public AActor
{
	GENERATED_BODY()

public:
	ARetrieveWaterSuppressVolume();

protected:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);
	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UPROPERTY(VisibleAnywhere, Category = "Retrieve|Water")
	TObjectPtr<UBoxComponent> Box;
};
