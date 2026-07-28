#pragma once

#include "CoreMinimal.h"
#include "World/RetrieveDoorBase.h"
#include "RetrieveInteractDoor.generated.h"

class URetrieveInteractionResponseComponent;

/**
 * 플레이어가 상호작용하면 열리고, 다시 상호작용하면 닫히는 토글 문.
 */
UCLASS(Blueprintable)
class RETRIEVE_API ARetrieveInteractDoor : public ARetrieveDoorBase
{
	GENERATED_BODY()

public:
	ARetrieveInteractDoor();

protected:
	virtual void BeginPlay() override;

	/** InteractionResponse.OnApplied 바인딩 — 상호작용 성공 시 문 토글. */
	UFUNCTION()
	void HandleInteracted(AActor* InteractionInstigator);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Door")
	TObjectPtr<URetrieveInteractionResponseComponent> InteractionResponse;
};
