#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RetrieveDialogueComponent.generated.h"

/**
 * 대화형 NPC라면 누구나(루멘 포함) 부착할 수 있는 재사용 가능한 대화 트리거.
 * URetrieveInteractionResponseComponent.OnApplied(호스트 전용)에 바인딩되어,
 * 상호작용한 Sovereign의 컨트롤러에 대화 뷰를 엽니다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RETRIEVE_API URetrieveDialogueComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URetrieveDialogueComponent();

	// URetrieveInteractionResponseComponent.OnApplied에 바인딩됨 (호스트 전용)
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Dialogue")
	void HandleInteract(AActor* Instigator);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void OpenConversationFor(AActor* Instigator);

	UPROPERTY(EditAnywhere, Category = "Retrieve|Dialogue")
	FText SpeakerDisplayName;

	UPROPERTY(EditAnywhere, Category = "Retrieve|Dialogue")
	bool bAutoBindResponseComponent = true;

private:
	bool bBoundToResponseComponent = false;
};
