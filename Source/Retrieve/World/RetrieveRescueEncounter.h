#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayTagContainer.h"
#include "RetrieveRescueEncounter.generated.h"

class ASpawnerBase;
class ARetrieveBonfireActor;
class USceneComponent;
struct FSpawnGroupClearedPayload;

UENUM(BlueprintType)
enum class ERetrieveRescueEncounterState : uint8
{
	EnemiesAlive,
	AwaitingRescueDialogue,
	RewardClaimed,
	MerchantAvailable
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FRetrieveRescueStateChangedSignature, ERetrieveRescueEncounterState, NewState);

UCLASS(Blueprintable)
class RETRIEVE_API ARetrieveRescueEncounter : public AActor
{
	GENERATED_BODY()

public:
	ARetrieveRescueEncounter();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Rescue")
	FName EncounterId;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Retrieve|Rescue")
	TObjectPtr<ASpawnerBase> EnemySpawner;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Retrieve|Rescue")
	TObjectPtr<AActor> RescueNPC;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Retrieve|Rescue")
	TObjectPtr<ARetrieveBonfireActor> DestinationBonfire;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Retrieve|Rescue")
	TObjectPtr<AActor> MerchantNPC;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Rescue", meta = (ClampMin = "0"))
	int32 GoldReward = 500;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Rescue")
	FGameplayTag SpawnGroupId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Rescue|Dialogue")
	TArray<FText> HelpDialogueLines;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Rescue|Dialogue")
	TArray<FText> ThankYouDialogueLines;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Retrieve|Rescue")
	ERetrieveRescueEncounterState State = ERetrieveRescueEncounterState::EnemiesAlive;

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Rescue")
	FRetrieveRescueStateChangedSignature OnStateChanged;

	/** 목표 마커에 표시할 퀘스트 제목. 비우면 "포로 구출". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Rescue")
	FText QuestTitle;

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Retrieve|Rescue")
	void FindNearestLoadedBonfire();

	void ResetForNewGame();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleRescueDialogueClosed(AActor* PlayerActor);

	UFUNCTION()
	void HandleSaveLoaded();

	UFUNCTION(BlueprintImplementableEvent, Category = "Retrieve|Rescue")
	void ReceiveRescueStateChanged(ERetrieveRescueEncounterState NewState);

private:
	void HandleSpawnGroupCleared(FGameplayTag Channel, const FSpawnGroupClearedPayload& Payload);
	void RestoreSavedState();
	void PersistState();
	void ApplyState();
	/** 현재 상태에 맞춰 수행/보상 마커를 갱신한다. */
	void RefreshObjectiveMarkers();
	void SetActorAvailable(AActor* Actor, bool bAvailable) const;
	void SetState(ERetrieveRescueEncounterState NewState, bool bPersist);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	FGameplayMessageListenerHandle ClearedHandle;
};