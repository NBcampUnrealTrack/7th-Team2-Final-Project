#include "World/RetrieveLostCargoEncounter.h"

#include "Components/Inventory/InventoryComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Components/World/RetrieveDialogueComponent.h"
#include "Components/World/RetrieveInteractionResponseComponent.h"
#include "Save/RetrieveSaveGame.h"
#include "Save/RetrieveSaveSubsystem.h"

ARetrieveLostCargoEncounter::ARetrieveLostCargoEncounter()
{
    PrimaryActorTick.bCanEverTick = false;
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    RequestDialogueLines = {
        NSLOCTEXT("RetrieveLostCargo", "Request01", "다리 건너편의 몬스터들이 제 짐을 훔쳐 갔어요."),
        NSLOCTEXT("RetrieveLostCargo", "Request02", "부탁이에요. 그 짐을 되찾아 주세요.")
    };
    SearchingDialogueLines = {
        NSLOCTEXT("RetrieveLostCargo", "Searching01", "몬스터들이 훔쳐 간 짐은 다리 건너편에 있을 거예요."),
        NSLOCTEXT("RetrieveLostCargo", "Searching02", "부디 조심해서 되찾아 와 주세요.")
    };
    ReturnDialogueLines = {
        NSLOCTEXT("RetrieveLostCargo", "Return01", "제 짐을 되찾아 오셨군요! 정말 감사합니다."),
        NSLOCTEXT("RetrieveLostCargo", "Return02", "약속한 보상이에요. 받아 주세요.")
    };
}

void ARetrieveLostCargoEncounter::BeginPlay()
{
    Super::BeginPlay();

    if (QuestNPC)
    {
        if (URetrieveDialogueComponent* Dialogue = QuestNPC->FindComponentByClass<URetrieveDialogueComponent>())
        {
            Dialogue->SpeakerTag = FGameplayTag();
            Dialogue->OnDialogueClosed.AddUniqueDynamic(this, &ThisClass::HandleNPCDialogueClosed);
        }
    }

    if (LostCargoActor)
    {
        CargoInteraction = LostCargoActor->FindComponentByClass<URetrieveInteractionResponseComponent>();
        if (CargoInteraction)
        {
            CargoInteraction->OnApplied.AddUniqueDynamic(this, &ThisClass::HandleCargoInteracted);
        }
    }

    if (UGameInstance* GI = GetGameInstance())
    {
        if (URetrieveSaveSubsystem* SaveSubsystem = GI->GetSubsystem<URetrieveSaveSubsystem>())
        {
            SaveSubsystem->OnLoadCompleted.AddUniqueDynamic(this, &ThisClass::HandleSaveLoaded);
        }
    }

    RestoreSavedState();
    ApplyState();
}

void ARetrieveLostCargoEncounter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (QuestNPC)
    {
        if (URetrieveDialogueComponent* Dialogue = QuestNPC->FindComponentByClass<URetrieveDialogueComponent>())
        {
            Dialogue->OnDialogueClosed.RemoveDynamic(this, &ThisClass::HandleNPCDialogueClosed);
        }
    }

    if (CargoInteraction)
    {
        CargoInteraction->OnApplied.RemoveDynamic(this, &ThisClass::HandleCargoInteracted);
    }

    if (UGameInstance* GI = GetGameInstance())
    {
        if (URetrieveSaveSubsystem* SaveSubsystem = GI->GetSubsystem<URetrieveSaveSubsystem>())
        {
            SaveSubsystem->OnLoadCompleted.RemoveDynamic(this, &ThisClass::HandleSaveLoaded);
        }
    }

    Super::EndPlay(EndPlayReason);
}

void ARetrieveLostCargoEncounter::HandleNPCDialogueClosed(AActor* PlayerActor)
{
    if (!HasAuthority() || !IsValid(PlayerActor))
    {
        return;
    }

    if (State == ERetrieveLostCargoState::AwaitingRequest)
    {
        SetState(ERetrieveLostCargoState::FindCargo, true);
        return;
    }

    if (State != ERetrieveLostCargoState::ReturnCargo)
    {
        return;
    }

    UInventoryComponent* Inventory = PlayerActor->FindComponentByClass<UInventoryComponent>();
    if (GoldReward > 0 && (!Inventory || !Inventory->AddCurrency(GoldReward)))
    {
        return;
    }

    SetState(ERetrieveLostCargoState::Completed, true);
}

void ARetrieveLostCargoEncounter::HandleCargoInteracted(AActor* InteractionInstigator)
{
    if (!HasAuthority() || State != ERetrieveLostCargoState::FindCargo || !IsValid(InteractionInstigator))
    {
        return;
    }

    SetState(ERetrieveLostCargoState::ReturnCargo, true);
}

void ARetrieveLostCargoEncounter::ResetForNewGame()
{
    State = ERetrieveLostCargoState::AwaitingRequest;
    ApplyState();
}

void ARetrieveLostCargoEncounter::HandleSaveLoaded()
{
    State = ERetrieveLostCargoState::AwaitingRequest;
    RestoreSavedState();
    ApplyState();
}

void ARetrieveLostCargoEncounter::RestoreSavedState()
{
    if (EncounterId.IsNone())
    {
        return;
    }

    const UGameInstance* GI = GetGameInstance();
    const URetrieveSaveSubsystem* SaveSubsystem = GI ? GI->GetSubsystem<URetrieveSaveSubsystem>() : nullptr;
    const URetrieveSaveGame* SaveGame = SaveSubsystem ? SaveSubsystem->GetCurrentSaveGame() : nullptr;
    const FRetrieveLostCargoSaveData* Saved = SaveGame ? SaveGame->LostCargoEncounters.Find(EncounterId) : nullptr;
    if (Saved)
    {
        State = static_cast<ERetrieveLostCargoState>(Saved->State);
    }
}

void ARetrieveLostCargoEncounter::PersistState()
{
    if (EncounterId.IsNone())
    {
        return;
    }

    UGameInstance* GI = GetGameInstance();
    URetrieveSaveSubsystem* SaveSubsystem = GI ? GI->GetSubsystem<URetrieveSaveSubsystem>() : nullptr;
    URetrieveSaveGame* SaveGame = SaveSubsystem ? SaveSubsystem->GetCurrentSaveGame() : nullptr;
    if (!SaveSubsystem || !SaveGame)
    {
        return;
    }

    SaveGame->LostCargoEncounters.FindOrAdd(EncounterId).State = static_cast<uint8>(State);
    SaveSubsystem->FlushWorldState();
}

void ARetrieveLostCargoEncounter::ApplyState()
{
    const bool bCompleted = State == ERetrieveLostCargoState::Completed;

    if (ACharacter* QuestCharacter = Cast<ACharacter>(QuestNPC))
    {
        if (UCharacterMovementComponent* Movement = QuestCharacter->GetCharacterMovement())
        {
            Movement->StopMovementImmediately();
            Movement->DisableMovement();
        }
        if (AController* Controller = QuestCharacter->GetController())
        {
            Controller->StopMovement();
        }
    }
    if (QuestNPC)
    {
        QuestNPC->SetActorHiddenInGame(bCompleted);
        QuestNPC->SetActorEnableCollision(!bCompleted);
        QuestNPC->SetActorTickEnabled(!bCompleted);

        if (URetrieveDialogueComponent* Dialogue = QuestNPC->FindComponentByClass<URetrieveDialogueComponent>())
        {
            Dialogue->SpeakerTag = FGameplayTag();
            switch (State)
            {
            case ERetrieveLostCargoState::AwaitingRequest:
                Dialogue->DefaultGreetingLines = RequestDialogueLines;
                break;
            case ERetrieveLostCargoState::FindCargo:
                Dialogue->DefaultGreetingLines = SearchingDialogueLines;
                break;
            case ERetrieveLostCargoState::ReturnCargo:
                Dialogue->DefaultGreetingLines = ReturnDialogueLines;
                break;
            case ERetrieveLostCargoState::Completed:
                Dialogue->DefaultGreetingLines.Reset();
                break;
            }
        }
    }

    if (LostCargoActor)
    {
        const bool bCargoCollected = State == ERetrieveLostCargoState::ReturnCargo || bCompleted;
        LostCargoActor->SetActorHiddenInGame(bCargoCollected);
        LostCargoActor->SetActorEnableCollision(State == ERetrieveLostCargoState::FindCargo);
        LostCargoActor->SetActorTickEnabled(!bCargoCollected);
    }

    OnStateChanged.Broadcast(State);
}

void ARetrieveLostCargoEncounter::SetState(ERetrieveLostCargoState NewState, bool bPersist)
{
    if (State == NewState)
    {
        return;
    }

    State = NewState;
    ApplyState();
    if (bPersist)
    {
        PersistState();
    }
}
