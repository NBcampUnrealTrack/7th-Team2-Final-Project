#include "Player/RetrievePlayerController.h"

#include "MVVMSubsystem.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "RetrievePlayerState.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Character/RetrieveCombatCharacter.h"
#include "Components/InventoryComponent.h"
#include "Components/RetrieveHealthComponent.h"
#include "Components/Widget.h"
#include "Components/WeaponComponent.h"
#include "Core/RetrieveGameMode.h"
#include "Core/RetrieveGameState.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Logging/RetrieveCheatManager.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "UI/Inventory/InventoryPanelWidget.h"
#include "UI/Map/RetrieveMinimapWidget.h"
#include "UI/Map/RetrieveWorldMapWidget.h"
#include "UI/RetrieveGamePanelWidget.h"
#include "UI/ViewModels/HUDViewModel.h"
#include "UI/ViewModels/PlayerStatusViewModel.h"
#include "UObject/ConstructorHelpers.h"
#include "View/MVVMView.h"

ARetrievePlayerController::ARetrievePlayerController(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bShowMouseCursor = false;
	CheatClass = URetrieveCheatManager::StaticClass();
	static ConstructorHelpers::FClassFinder<UUserWidget> HUDWidgetFinder(TEXT("/Game/Retrieve/UI/WBP_HUD"));
	if (HUDWidgetFinder.Succeeded())
	{
		HUDClass = HUDWidgetFinder.Class;
	}
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
	RemoveActivePanelImmediately();

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

	if (ActiveToastManager)
	{
		ActiveToastManager->RemoveFromParent();
		ActiveToastManager = nullptr;
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

bool ARetrievePlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	if (Params.Event == IE_Pressed)
	{
		if (ActivePanel && Params.Key == EKeys::Escape)
		{
			CloseActivePanel();
			return true;
		}

		if (TryHandleMinimapShortcut(Params.Key))
		{
			return true;
		}

		if (TryHandlePanelShortcut(Params.Key))
		{
			return true;
		}

		if (TryHandleConsumableSlotShortcut(Params.Key))
		{
			return true;
		}
	}

	return Super::InputKey(Params);
}

void ARetrievePlayerController::AcknowledgePossession(APawn* InPawn)
{
	Super::AcknowledgePossession(InPawn);
	
	TryBindHealthToHUD();
}

void ARetrievePlayerController::HandleSessionStateChanged(ERetrieveSessionState NewState)
{
	RemoveActivePanelImmediately();
	SwapActiveWidget(NewState);
	UpdateInputMode(NewState);
}

void ARetrievePlayerController::SwapActiveWidget(ERetrieveSessionState NewState)
{
	if (ActiveTopLevelWidget)
	{
		ClearHUDViewModel();
		ActiveTopLevelWidget->RemoveFromParent();
		ActiveTopLevelWidget = nullptr;
	}

	const TSubclassOf<UUserWidget> WidgetClass = ResolveWidgetClass(NewState);
	if (WidgetClass)
	{
		ActiveTopLevelWidget = CreateWidget<UUserWidget>(this, WidgetClass);
		if (ActiveTopLevelWidget)
		{
			if (NewState == ERetrieveSessionState::InGame)
			{
				EnsureHUDViewModel();
			}
			ActiveTopLevelWidget->AddToViewport();
			if (NewState == ERetrieveSessionState::InGame)
			{
				TryBindHealthToHUD();
			}
		}
	}

	// ── 토스트 매니저: InGame 상태에서만 활성화 ──────────────────────
	// HUD(ActiveTopLevelWidget)와 독립적으로 관리된다.
	// ZOrder=10 으로 HUD(기본 0) 위에 렌더링.
	if (ActiveToastManager)
	{
		ActiveToastManager->RemoveFromParent();
		ActiveToastManager = nullptr;
	}

	if (NewState == ERetrieveSessionState::InGame && ToastManagerClass)
	{
		ActiveToastManager = CreateWidget<UUserWidget>(this, ToastManagerClass);
		if (ActiveToastManager)
		{
			ActiveToastManager->AddToViewport(10);
		}
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
				FInputModeGameOnly Mode;
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

void ARetrievePlayerController::OpenExclusivePanel(TSubclassOf<URetrieveGamePanelWidget> PanelClass, FKey ToggleKey)
{
	if (!PanelClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to open panel: PanelClass is empty."));
		return;
	}

	if (ActivePanel && ActivePanelClass == PanelClass)
	{
		CloseActivePanel();
		return;
	}

	if (ActivePanel)
	{
		RemoveActivePanelImmediately();
	}

	URetrieveGamePanelWidget* NewPanel = CreateWidget<URetrieveGamePanelWidget>(this, PanelClass);
	if (!NewPanel)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to create panel: %s"), *GetNameSafe(PanelClass));
		return;
	}

	NewPanel->SetIsFocusable(true);
	NewPanel->ToggleKey = ToggleKey;
	NewPanel->OnCloseRequested.AddDynamic(this, &ThisClass::HandleActivePanelCloseRequested);
	NewPanel->OnUIVFXFinished.AddDynamic(this, &ThisClass::HandleActivePanelCloseVFXFinished);

	if (UInventoryPanelWidget* InventoryPanel = Cast<UInventoryPanelWidget>(NewPanel))
	{
		InventoryPanel->InitializeInventoryPanel(GetPawnInventoryComponent(), GetPawnWeaponComponent());
	}

	ActivePanel = NewPanel;
	ActivePanelClass = PanelClass;

	NewPanel->AddToViewport(PanelZOrder);
	CenterActiveWorldMapPanel();

	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(NewPanel->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
	bShowMouseCursor = true;

	NewPanel->SetKeyboardFocus();
	NewPanel->PlayPanelOpenVFX();
}

void ARetrievePlayerController::CloseActivePanel()
{
	if (!ActivePanel || bActivePanelClosing)
	{
		return;
	}

	bActivePanelClosing = true;
	if (!ActivePanel->PlayPanelCloseVFX())
	{
		RemoveActivePanelImmediately();
	}
}

void ARetrievePlayerController::RemoveActivePanelImmediately()
{
	if (!ActivePanel)
	{
		bActivePanelClosing = false;
		return;
	}

	ActivePanel->OnCloseRequested.RemoveDynamic(this, &ThisClass::HandleActivePanelCloseRequested);
	ActivePanel->OnUIVFXFinished.RemoveDynamic(this, &ThisClass::HandleActivePanelCloseVFXFinished);
	ActivePanel->RemoveFromParent();
	ActivePanel = nullptr;
	ActivePanelClass = nullptr;
	bActivePanelClosing = false;

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	bShowMouseCursor = false;
}

void ARetrievePlayerController::ToggleMinimapRotationMode()
{
	if (URetrieveMinimapWidget* MinimapWidget = FindMinimapWidgetInHUD())
	{
		MinimapWidget->ToggleRotationMode();
	}
}

void ARetrievePlayerController::HandleActivePanelCloseRequested()
{
	CloseActivePanel();
}

void ARetrievePlayerController::HandleActivePanelCloseVFXFinished(FGameplayTag EffectTag)
{
	if (EffectTag == RetrieveGameplayTags::UI_VFX_Panel_Close)
	{
		RemoveActivePanelImmediately();
	}
}

bool ARetrievePlayerController::TryHandleMinimapShortcut(FKey Key)
{
	if (bEnableMinimapRotationShortcut && MinimapRotationKey.IsValid() && Key == MinimapRotationKey)
	{
		ToggleMinimapRotationMode();
		return true;
	}

	return false;
}

bool ARetrievePlayerController::TryHandlePanelShortcut(FKey Key)
{
	for (const FRetrievePanelShortcutConfig& ShortcutConfig : PanelShortcuts)
	{
		if (ShortcutConfig.Key != Key)
		{
			continue;
		}

		if (!CanOpenPanel(ShortcutConfig))
		{
			return true;
		}

		OpenExclusivePanel(ShortcutConfig.PanelClass, ShortcutConfig.Key);
		return true;
	}

	return false;
}

bool ARetrievePlayerController::TryHandleConsumableSlotShortcut(FKey Key)
{
	const int32 SlotKey = Key == EKeys::Four ? 4 : (Key == EKeys::Five ? 5 : INDEX_NONE);
	if (SlotKey == INDEX_NONE)
	{
		return false;
	}

	if (UInventoryComponent* InventoryComponent = GetPawnInventoryComponent())
	{
		InventoryComponent->UseConsumableSlot(SlotKey);
	}

	return true;
}

bool ARetrievePlayerController::CanOpenPanel(const FRetrievePanelShortcutConfig& ShortcutConfig) const
{
	if (!ShortcutConfig.PanelClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid panel shortcut: %s has no PanelClass."), *ShortcutConfig.Key.ToString());
		return false;
	}

	if (!ShortcutConfig.bRequiresInventoryOpenPermission)
	{
		return true;
	}

	const UInventoryComponent* InventoryComponent = GetPawnInventoryComponent();
	if (!InventoryComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to open inventory panel: pawn has no InventoryComponent."));
		return false;
	}

	return InventoryComponent->CanOpenInventory();
}

void ARetrievePlayerController::CenterActiveWorldMapPanel()
{
	if (!bCenterWorldMapOnOpen)
	{
		return;
	}

	if (URetrieveWorldMapWidget* WorldMapWidget = Cast<URetrieveWorldMapWidget>(ActivePanel))
	{
		WorldMapWidget->CenterOnPlayer();
	}
}


URetrieveMinimapWidget* ARetrievePlayerController::FindMinimapWidgetInHUD() const
{
	if (!ActiveTopLevelWidget)
	{
		return nullptr;
	}

	if (URetrieveMinimapWidget* DirectMinimapWidget = Cast<URetrieveMinimapWidget>(ActiveTopLevelWidget))
	{
		return DirectMinimapWidget;
	}

	if (UWidget* RootWidget = ActiveTopLevelWidget->GetRootWidget())
	{
		if (URetrieveMinimapWidget* RootMinimapWidget = Cast<URetrieveMinimapWidget>(RootWidget))
		{
			return RootMinimapWidget;
		}
	}

	URetrieveMinimapWidget* FoundMinimapWidget = nullptr;
	if (ActiveTopLevelWidget->WidgetTree)
	{
		ActiveTopLevelWidget->WidgetTree->ForEachWidget([&FoundMinimapWidget](UWidget* Widget)
		{
			if (!FoundMinimapWidget)
			{
				FoundMinimapWidget = Cast<URetrieveMinimapWidget>(Widget);
			}
		});
	}

	return FoundMinimapWidget;
}

UInventoryComponent* ARetrievePlayerController::GetPawnInventoryComponent() const
{
	const APawn* ControlledPawn = GetPawn();
	return ControlledPawn ? ControlledPawn->FindComponentByClass<UInventoryComponent>() : nullptr;
}

UWeaponComponent* ARetrievePlayerController::GetPawnWeaponComponent() const
{
	const APawn* ControlledPawn = GetPawn();
	return ControlledPawn ? ControlledPawn->FindComponentByClass<UWeaponComponent>() : nullptr;
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


void ARetrievePlayerController::EnsureHUDViewModel()
{
	if (!ActiveTopLevelWidget)
	{
		return;
	}
	if (!HUDViewModelInstance)
	{
		HUDViewModelInstance = NewObject<UHUDViewModel>(this);
	}
	
	UMVVMSubsystem* MVVM = GEngine ? GEngine->GetEngineSubsystem<UMVVMSubsystem>() : nullptr;
	if (!MVVM)
	{
		return;
	}
	
	UMVVMView* View = MVVM->GetViewFromUserWidget(ActiveTopLevelWidget);
	if (!View)
	{
		return;
	}
	
	View->SetViewModel(HUDViewModelBindingName, HUDViewModelInstance);
}

void ARetrievePlayerController::TryBindHealthToHUD()
{
	if (!HUDViewModelInstance)
	{
		return;
	}
	
	UPlayerStatusViewModel* PlayerStatus = HUDViewModelInstance->GetPlayerStatus();
	if (!PlayerStatus)
	{
		return;
	}
	
	ARetrieveCombatCharacter* Combatant = Cast<ARetrieveCombatCharacter>(GetPawn());
	if (!Combatant)
	{
		return;
	}
	
	if (URetrieveHealthComponent* Health = Combatant->GetHealthComponent())
	{
		PlayerStatus->BindToHealth(Health);
	}
}

void ARetrievePlayerController::ClearHUDViewModel()
{
	if (HUDViewModelInstance)
	{
		if (UPlayerStatusViewModel* PlayerStatus = HUDViewModelInstance->GetPlayerStatus())
		{
			PlayerStatus->UnbindFromHealth();
		}
	}
}
