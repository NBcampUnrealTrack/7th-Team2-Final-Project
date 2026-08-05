#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RetrieveLostCargoEncounter.generated.h"

class URetrieveInteractionResponseComponent;
class USceneComponent;

UENUM(BlueprintType)
enum class ERetrieveLostCargoState : uint8
{
    AwaitingRequest,
    FindCargo,
    ReturnCargo,
    Completed
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FRetrieveLostCargoStateChangedSignature, ERetrieveLostCargoState, NewState);

UCLASS(Blueprintable)
class RETRIEVE_API ARetrieveLostCargoEncounter : public AActor
{
    GENERATED_BODY()

public:
    ARetrieveLostCargoEncounter();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Lost Cargo")
    FName EncounterId = TEXT("LostCargo_Bridge_01");

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Retrieve|Lost Cargo")
    TObjectPtr<AActor> QuestNPC;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Retrieve|Lost Cargo")
    TObjectPtr<AActor> LostCargoActor;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Lost Cargo", meta = (ClampMin = "0"))
    int32 GoldReward = 500;

    /** 목표 마커에 표시할 퀘스트 제목. 비우면 "잃어버린 화물". */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Lost Cargo")
    FText QuestTitle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Lost Cargo|Dialogue")
    TArray<FText> RequestDialogueLines;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Lost Cargo|Dialogue")
    TArray<FText> SearchingDialogueLines;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Lost Cargo|Dialogue")
    TArray<FText> ReturnDialogueLines;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Retrieve|Lost Cargo")
    ERetrieveLostCargoState State = ERetrieveLostCargoState::AwaitingRequest;

    UPROPERTY(BlueprintAssignable, Category = "Retrieve|Lost Cargo")
    FRetrieveLostCargoStateChangedSignature OnStateChanged;

    void ResetForNewGame();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION()
    void HandleNPCDialogueClosed(AActor* PlayerActor);

    UFUNCTION()
    void HandleCargoInteracted(AActor* InteractionInstigator);

    UFUNCTION()
    void HandleSaveLoaded();

private:
    /** 현재 상태에 맞춰 의뢰/수행/보상 마커를 갱신한다. */
    void RefreshObjectiveMarkers();

    void RestoreSavedState();
    void PersistState();
    void ApplyState();
    void SetState(ERetrieveLostCargoState NewState, bool bPersist);

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(Transient)
    TObjectPtr<URetrieveInteractionResponseComponent> CargoInteraction;
};
