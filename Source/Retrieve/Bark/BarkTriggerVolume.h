#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"
#include "BarkTriggerVolume.generated.h"

class UBoxComponent;

/** 로컬 플레이어가 진입하면 (KeyTag 기준으로) Manual Bark를 발동하는, 레벨에 배치하는 볼륨입니다. */
UCLASS()
class RETRIEVE_API ABarkTriggerVolume : public AActor
{
	GENERATED_BODY()

public:
	ABarkTriggerVolume();

protected:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	                        int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);

	/** 발동할 Manual 행의 KeyTag (FBarkRow.KeyTag와 매칭). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bark")
	FGameplayTag BarkKeyTag;

	/** true면 1회만 발동합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bark")
	bool bOnce = true;

	UPROPERTY(VisibleAnywhere, Category = "Bark")
	TObjectPtr<UBoxComponent> Box;

private:
	bool bFired = false;
};
