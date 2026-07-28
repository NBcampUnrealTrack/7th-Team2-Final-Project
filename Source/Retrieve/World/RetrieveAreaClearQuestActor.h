#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayTagContainer.h"
#include "RetrieveAreaClearQuestActor.generated.h"

struct FSpawnGroupClearedPayload;

/**
 * 레벨에 배치하는 호스트 권한 "구역 클리어 → 퀘스트 스텝 완료" 액터.
 * 스포너(ASpawnerBase)가 살아있는 스폰을 모두 잃으면 발행하는
 * Channel.Enemy.SpawnGroupCleared 신호를 구독하고, 페이로드의 SpawnGroupId가
 * 이 액터에 설정된 값과 일치하면 CompleteStepTag를 완료합니다.
 * 
 * 에디터에서 다음 항목을 설정하세요:
 *  - SpawnGroupId : 대상 스포너의 SpawnGroupId와 동일하게 설정
 *  - CompleteStepTag : 그룹이 클리어되면 완료할 퀘스트 스텝 (예: Quest.Step.WolvesDefeated)
 */
UCLASS()
class RETRIEVE_API ARetrieveAreaClearQuestActor : public AActor
{
	GENERATED_BODY()

public:
	ARetrieveAreaClearQuestActor();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 그룹 클리어 신호를 수신하고, 그룹이 일치하면 호스트 권한으로 스텝을 완료합니다. */
	void OnSpawnGroupCleared(FGameplayTag Channel, const FSpawnGroupClearedPayload& Payload);

	/** 완료를 유발하는 스폰 그룹. 대상 스포너의 SpawnGroupId와 동일하게 설정합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Story")
	FGameplayTag SpawnGroupId;

	/** 그룹이 클리어되면 완료할 퀘스트 스텝. 예: Quest.Step.WolvesDefeated */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Story", meta = (Categories = "Quest.Step"))
	FGameplayTag CompleteStepTag;

	/** true면 1회만 발동합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Story")
	bool bOnce = true;

	UPROPERTY(VisibleAnywhere, Category = "Retrieve|Story")
	TObjectPtr<USceneComponent> Root;

private:
	FGameplayMessageListenerHandle ClearedHandle;
	bool bFired = false;
};
