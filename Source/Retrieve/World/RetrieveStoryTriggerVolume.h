#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"
#include "RetrieveStoryTriggerVolume.generated.h"

class UBoxComponent;

/**
 * 레벨에 배치하는 호스트 권한 스토리 트리거 볼륨.
 * 호스트 폰이 진입하면 설정에 따라 (1) 대상 NPC의 대화를 자동으로 열거나
 * (2) 퀘스트 스텝을 완료합니다. 둘 다 설정하면 둘 다 실행됩니다.
 * 스토리/퀘스트 진행 권한은 호스트에게만 있습니다(ABarkTriggerVolume과 같은
 * 볼륨 패턴이지만, 로컬이 아닌 호스트 권한으로 동작합니다).
 */
UCLASS()
class RETRIEVE_API ARetrieveStoryTriggerVolume : public AActor
{
	GENERATED_BODY()

public:
	ARetrieveStoryTriggerVolume();

protected:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	                        int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);

	/** 진입 시 이 NPC의 URetrieveDialogueComponent 대화를 자동으로 엽니다(선택). 예: 동굴의 Lumen. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Retrieve|Story")
	TObjectPtr<AActor> DialogueTarget;

	/** 진입 시 완료할 퀘스트 스텝(선택). 예: Quest.Step.ExitedCave */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Story", meta = (Categories = "Quest.Step"))
	FGameplayTag CompleteStepTag;

	/** true면 1회만 발동합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Story")
	bool bOnce = true;

	UPROPERTY(VisibleAnywhere, Category = "Retrieve|Story")
	TObjectPtr<UBoxComponent> Box;

private:
	bool bFired = false;
};
