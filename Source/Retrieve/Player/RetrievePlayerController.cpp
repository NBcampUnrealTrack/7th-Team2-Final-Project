#include "Player/RetrievePlayerController.h"

#include "MVVMSubsystem.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "RetrievePlayerState.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Character/RetrieveCombatCharacter.h"
#include "Components/AttackFeedbackComponent.h"
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
#include "UI/HUD/RetrieveElementGaugeWidget.h"
#include "UI/HUD/RetrieveBossHPBarWidget.h"
#include "UI/ViewModels/BossStatusViewModel.h"
#include "UI/ViewModels/ElementGaugeViewModel.h"
#include "UI/ViewModels/HUDViewModel.h"
#include "UI/ViewModels/PlayerStatusViewModel.h"
#include "Components/ElementGaugeComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "View/MVVMView.h"

namespace
{
	UWidget* FindBossBarWidget(UUserWidget* TopLevelWidget)
	{
		if (!TopLevelWidget)
		{
			return nullptr;
		}

		UWidget* BossBarWidget = TopLevelWidget->GetWidgetFromName(TEXT("WBP_BossHPBar"));
		if (!BossBarWidget && TopLevelWidget->WidgetTree)
		{
			TopLevelWidget->WidgetTree->ForEachWidget([&BossBarWidget](UWidget* Widget)
			{
				if (!BossBarWidget && Widget && Widget->GetFName() == TEXT("WBP_BossHPBar"))
				{
					BossBarWidget = Widget;
				}
			});
		}

		return BossBarWidget;
	}
}

ARetrievePlayerController::ARetrievePlayerController(const FObjectInitializer& ObjectInitializer) : Super(
	ObjectInitializer)
{
	bShowMouseCursor = false;
	CheatClass = URetrieveCheatManager::StaticClass();
	AttackFeedbackComponent  = CreateDefaultSubobject<UAttackFeedbackComponent>(TEXT("AttackFeedbackComponent"));
}

ARetrievePlayerState* ARetrievePlayerController::GetRetrievePlayerState() const
{
	return CastChecked<ARetrievePlayerState>(PlayerState, ECastCheckedType::NullAllowed);
}

UActorComponent* ARetrievePlayerController::GetInteractorComponent() const
{
	if (UClass* ComponentClass = InteractorComponentClass.LoadSynchronous())
	{
		return FindComponentByClass(ComponentClass);
	}
	return nullptr;
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

	// 패널이 열려있는 동안은 전투/어빌리티 입력을 차단한다.
	// 월드맵의 좌클릭 더블클릭 판정이 공격 콤보 입력과 충돌하는 것을 방지.
	if (!ActivePanel)
	{
		if (ARetrievePlayerState* RetrievePS = GetRetrievePlayerState())
		{
			if (URetrieveAbilitySystemComponent* RetrieveASC = RetrievePS->GetRetrieveAbilitySystemComponent())
			{
				const bool bGamePaused = IsPaused();
				RetrieveASC->ProcessAbilityInput(DeltaTime, bGamePaused);
			}
		}
	}
	
	if (const APawn* ControlledPawn = GetPawn())
	{
		if (ControlledPawn->GetVelocity().SizeSquared2D() > 25.f)
		{
			if (const UWorld* World = GetWorld())
			{
				LastInputRealTimeSeconds = World->GetRealTimeSeconds();
			}
		}
	}

}

bool ARetrievePlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	if (UWorld* World = GetWorld())
	{
		LastInputRealTimeSeconds = World->GetRealTimeSeconds();
	}

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

		// 패널이 열려있는 동안은 위에서 처리되지 않은 키보드/게임패드 입력을 차단한다.
		// 마우스 버튼은 제외(월드맵 패닝·클릭은 Slate UI가 직접 처리).
		if (ActivePanel && !Params.Key.IsMouseButton())
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
	TryBindElementGaugeToHUD();
	if (AttackFeedbackComponent)
	{
		AttackFeedbackComponent->HandlePossessedPawnChanged(InPawn);
	}
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
			// AddToViewport 전에 인스턴스를 미리 생성 — 자식 UserWidget의 NativeConstruct에서
			// GetHUDViewModel()이 null이 되지 않도록 보장한다.
			if (NewState == ERetrieveSessionState::InGame && !HUDViewModelInstance)
			{
				HUDViewModelInstance = NewObject<UHUDViewModel>(this);
			}
			ActiveTopLevelWidget->AddToViewport();
			if (NewState == ERetrieveSessionState::InGame)
			{
				EnsureHUDViewModel();
				TryBindHealthToHUD();
				TryBindElementGaugeToHUD();
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

float ARetrievePlayerController::GetSecondsSinceLastInput() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.f;
	}
	return static_cast<float>(World->GetRealTimeSeconds() - LastInputRealTimeSeconds);
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

void ARetrievePlayerController::Server_RequestRecallLumen_Implementation()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	
	FRetrieveLumenCommandPayload Message;
	Message.CommandTag = RetrieveGameplayTags::Channel_Lumen_Command_Recall;
	Message.Instigator = GetPawn();
	UGameplayMessageSubsystem::Get(World).BroadcastMessage(RetrieveGameplayTags::Channel_Lumen_Command_Recall, Message);
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

	// 자식 ViewModel도 명시적으로 등록 — WBP_HPBar처럼 클래스 직접 참조 방식과 호환
	View->SetViewModel(TEXT("PlayerStatus"),  HUDViewModelInstance->GetPlayerStatus());
	View->SetViewModel(TEXT("ElementGauge"),  HUDViewModelInstance->GetElementGauge());
	View->SetViewModel(TEXT("BossStatus"),    HUDViewModelInstance->GetBossStatus());

	BindBossStatusViewModelToBossBarWidget();
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

	APawn* OwnerPawn = GetPawn();
	if (!OwnerPawn)
	{
		return;
	}

	// 모든 Pawn 공통 처리는 컴포넌트 조회로 통일 (TEAMRULE 진입 규약).
	// Sovereign이 ALS 가지로 옮겨가도, 어떤 자식 가지의 Pawn이든 HealthComponent 보유 시 작동.
	if (URetrieveHealthComponent* Health = OwnerPawn->FindComponentByClass<URetrieveHealthComponent>())
	{
		PlayerStatus->BindToHealth(Health);
	}
}

void ARetrievePlayerController::TryBindElementGaugeToHUD()
{
	if (!HUDViewModelInstance)
	{
		return;
	}

	UElementGaugeViewModel* ElementGaugeVM = HUDViewModelInstance->GetElementGauge();
	if (!ElementGaugeVM)
	{
		return;
	}

	APawn* OwnerPawn = GetPawn();
	if (!OwnerPawn)
	{
		return;
	}

	if (UElementGaugeComponent* GaugeComp = OwnerPawn->FindComponentByClass<UElementGaugeComponent>())
	{
		ElementGaugeVM->BindToGauge(GaugeComp);
	}

	// 원소 모드 태그는 ASC(PlayerState)에 있으므로 별도로 바인딩
	if (ARetrievePlayerState* PS = GetPlayerState<ARetrievePlayerState>())
	{
		if (UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent())
		{
			ElementGaugeVM->BindToASC(ASC);
		}
	}
}

void ARetrievePlayerController::TryBindBossToHUD(URetrieveHealthComponent* BossHealth, FText BossName)
{
	if (!HUDViewModelInstance)
	{
		return;
	}

	if (UBossStatusViewModel* BossVM = HUDViewModelInstance->GetBossStatus())
	{
		BindBossStatusViewModelToBossBarWidget();
		BossVM->BindToBoss(BossHealth, BossName);
		ApplyBossStatusToBossBarWidget();
	}
}

void ARetrievePlayerController::BindBossStatusViewModelToBossBarWidget()
{
	if (!ActiveTopLevelWidget || !HUDViewModelInstance)
	{
		return;
	}

	UWidget* BossBarWidget = FindBossBarWidget(ActiveTopLevelWidget);

	if (URetrieveBossHPBarWidget* BossHPBar = Cast<URetrieveBossHPBarWidget>(BossBarWidget))
	{
		BossHPBar->SetBossStatusViewModel(HUDViewModelInstance->GetBossStatus());
	}

	ApplyBossStatusToBossBarWidget();
}

void ARetrievePlayerController::ApplyBossStatusToBossBarWidget()
{
	if (!ActiveTopLevelWidget || !HUDViewModelInstance)
	{
		return;
	}

	UWidget* BossBarWidget = FindBossBarWidget(ActiveTopLevelWidget);
	if (!BossBarWidget)
	{
		return;
	}

	const UBossStatusViewModel* BossVM = HUDViewModelInstance->GetBossStatus();
	const ESlateVisibility DesiredVisibility = BossVM
		? BossVM->GetSlateVisibility()
		: ESlateVisibility::Collapsed;

	BossBarWidget->SetVisibility(DesiredVisibility);
}

void ARetrievePlayerController::ClearHUDViewModel()
{
	if (HUDViewModelInstance)
	{
		if (UPlayerStatusViewModel* PlayerStatus = HUDViewModelInstance->GetPlayerStatus())
		{
			PlayerStatus->UnbindFromHealth();
		}

		if (UElementGaugeViewModel* ElementGaugeVM = HUDViewModelInstance->GetElementGauge())
		{
			ElementGaugeVM->UnbindFromGauge();
			ElementGaugeVM->UnbindFromASC();
		}

		if (UBossStatusViewModel* BossVM = HUDViewModelInstance->GetBossStatus())
		{
			BossVM->UnbindFromBoss();
		}
	}
}
