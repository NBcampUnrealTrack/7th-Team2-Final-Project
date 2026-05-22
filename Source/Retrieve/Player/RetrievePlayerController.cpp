#include "Player/RetrievePlayerController.h"

#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "RetrievePlayerState.h"
#include "Blueprint/UserWidget.h"
#include "Core/RetrieveGameMode.h"
#include "Core/RetrieveGameState.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Messaging/RetrieveMessageTypes.h"

ARetrievePlayerController::ARetrievePlayerController(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

ARetrievePlayerState* ARetrievePlayerController::GetRetrievePlayerState() const
{
	return CastChecked<ARetrievePlayerState>(PlayerState, ECastCheckedType::NullAllowed);
}

void ARetrievePlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	if (!IsLocalController())
	{
		return;
	}
	
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	
	SessionListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FRetrieveSessionStatePayload>(
		RetrieveGameplayTags::Channel_Session_StateChanged,
		[WeakThis = TWeakObjectPtr<ARetrievePlayerController>(this)]
		(FGameplayTag /*Channel*/, const FRetrieveSessionStatePayload& Payload)
		{
			if (ARetrievePlayerController* RetrievePC = WeakThis.Get())
			{
				RetrievePC->HandleSessionStateChanged(Payload.NewState);
			}
		});
	
	if (ARetrieveGameState* GS = World->GetGameState<ARetrieveGameState>())
	{
		HandleSessionStateChanged(GS->GetSessionState());
	}
}

void ARetrievePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (SessionListener.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			UGameplayMessageSubsystem::Get(World).UnregisterListener(SessionListener);
		}
		SessionListener = FGameplayMessageListenerHandle();
	}
	
	if (ActiveTopLevelWidget)
	{
		ActiveTopLevelWidget->RemoveFromParent();
		ActiveTopLevelWidget = nullptr;
	}
	
	Super::EndPlay(EndPlayReason);
}

void ARetrievePlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	
	if (ARetrievePlayerState* RetrievePS = GetRetrievePlayerState())
	{
		if (URetrieveAbilitySystemComponent* RetrieveASC = RetrievePS->GetRetrieveAbilitySystemComponent())
		{
			const bool bGamePaused = IsPaused();
			RetrieveASC->ProcessAbilityInput(DeltaTime, bGamePaused);
		}
	}
}

void ARetrievePlayerController::HandleSessionStateChanged(ERetrieveSessionState NewState)
{
	SwapActiveWidget(NewState);
	UpdateInputMode(NewState);
}

void ARetrievePlayerController::SwapActiveWidget(ERetrieveSessionState NewState)
{
	if (ActiveTopLevelWidget)
	{
		ActiveTopLevelWidget->RemoveFromParent();
		ActiveTopLevelWidget = nullptr;
	}
	
	const TSubclassOf<UUserWidget> WidgetClass = ResolveWidgetClass(NewState);
	if (!WidgetClass)
	{
		return;
	}
	
	ActiveTopLevelWidget = CreateWidget<UUserWidget>(this, WidgetClass);
	if (ActiveTopLevelWidget)
	{
		ActiveTopLevelWidget->AddToViewport();
	}
}

void ARetrievePlayerController::UpdateInputMode(ERetrieveSessionState NewState)
{
	switch (NewState)
	{
		case ERetrieveSessionState::MainMenu:
		case ERetrieveSessionState::Result:
			{
				FInputModeUIOnly Mode;
				Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
				SetInputMode(Mode);
				bShowMouseCursor = true;
				break;
			}
	
		case ERetrieveSessionState::InGame:
			{
				FInputModeGameAndUI Mode;
				Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
				Mode.SetHideCursorDuringCapture(true);
				SetInputMode(Mode);
				bShowMouseCursor = false;
				break;
			}
	
		case ERetrieveSessionState::Loading:
		default:
			{
				break;
			}
		}
}

TSubclassOf<UUserWidget> ARetrievePlayerController::ResolveWidgetClass(ERetrieveSessionState State) const
{
	switch (State)
	{
		case ERetrieveSessionState::MainMenu:
			return MainMenuClass;
		
		case ERetrieveSessionState::InGame:
			return HUDClass;
		
		case ERetrieveSessionState::Result:
			return ResultClass;
		
		case ERetrieveSessionState::Loading:
		default:
			return nullptr;
	}
}


void ARetrievePlayerController::Server_RequestNewGame_Implementation()
{
	if (ARetrieveGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ARetrieveGameMode>() : nullptr)
	{
		GM->HandleNewGame(this);
	}
}


void ARetrievePlayerController::Server_RequestRetry_Implementation()
{
	if (ARetrieveGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ARetrieveGameMode>() : nullptr)
	{
		GM->HandleRetry(this);
	}
}

void ARetrievePlayerController::Server_RequestQuitToMenu_Implementation()
{
	if (ARetrieveGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ARetrieveGameMode>() : nullptr)
	{
		GM->HandleQuitToMenu(this);
	}
}