#include "Player/RetrievePlayerController.h"

#include "MVVMSubsystem.h"
#include "Settings/RetrieveSettingsSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "RetrievePlayerState.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Combat/AttackFeedbackComponent.h"
#include "Components/Inventory/InventoryComponent.h"
#include "Components/Combat/RetrieveHealthComponent.h"
#include "Components/Widget.h"
#include "Components/Player/WeaponComponent.h"
#include "Core/RetrieveGameMode.h"
#include "Core/RetrieveGameState.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Logging/RetrieveCheatManager.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "UI/Inventory/InventoryPanelWidget.h"
#include "UI/HUD/RetrieveQuickSlotWheelWidget.h"
#include "UI/Shop/ShopPanelWidget.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Shop/RetrieveShopDefinitionAsset.h"
#include "Components/World/RetrieveDialogueComponent.h"
#include "Components/World/RetrieveShopComponent.h"
#include "UI/Map/RetrieveMinimapWidget.h"
#include "UI/Map/RetrieveWorldMapWidget.h"
#include "UI/RetrieveGamePanelWidget.h"
#include "UI/ViewModels/ConversationViewModel.h"
#include "UI/ViewModels/QuestTrackerViewModel.h"
#include "UI/HUD/RetrieveBossHPBarWidget.h"
#include "UI/ViewModels/BossStatusViewModel.h"
#include "UI/ViewModels/ElementGaugeViewModel.h"
#include "UI/ViewModels/HUDViewModel.h"
#include "UI/ViewModels/PlayerStatusViewModel.h"
#include "Components/Element/ElementGaugeComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UI/Loading/RetrieveLoadingScreenWidget.h"
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
	AttackFeedbackComponent = CreateDefaultSubobject<UAttackFeedbackComponent>(TEXT("AttackFeedbackComponent"));
	SettingsPanelClass = TSoftClassPtr<URetrieveGamePanelWidget>(
		FSoftObjectPath(TEXT("/Game/Retrieve/UI/Settings/WBP_SettingsScreen.WBP_SettingsScreen_C")));
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

	// 저장된 설정 중 World/Controller 종속 항목(오디오 믹스·진동)을 이 시점에 적용한다.
	// (전역 그래픽/감마/색맹은 서브시스템 Initialize에서 이미 적용됨)
	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (URetrieveSettingsSubsystem* SettingsSubsystem = LP->GetSubsystem<URetrieveSettingsSubsystem>())
		{
			SettingsSubsystem->ApplyWorldSettings(World);
			SettingsSubsystem->ApplyControllerSettings(this);
		}
	}

	// 상점 포커스 카메라를 미리 스폰해 전환 시점의 스폰 히치를 제거한다.
	if (!ShopFocusCameraActor)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ShopFocusCameraActor = World->SpawnActor<ACameraActor>(
			ACameraActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	}

	SessionListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FRetrieveSessionStatePayload>(
		RetrieveGameplayTags::Channel_Session_StateChanged,
		[WeakThis = TWeakObjectPtr<ARetrievePlayerController>(this)]
	(FGameplayTag /*Channel*/, const FRetrieveSessionStatePayload& Payload)
		{
			if (ARetrievePlayerController* RetrievePC = WeakThis.Get())
			{
				RetrievePC->HandleSessionStateChanged(Payload.PreviousState, Payload.NewState);
			}
		});

	ShopCommandHandle = UGameplayMessageSubsystem::Get(World).RegisterListener<FRetrieveLumenCommandPayload>(
		RetrieveGameplayTags::Channel_Shop_OpenShop,
		[WeakThis = TWeakObjectPtr<ARetrievePlayerController>(this)]
	(FGameplayTag /*Channel*/, const FRetrieveLumenCommandPayload& Payload)
		{
			if (ARetrievePlayerController* PC = WeakThis.Get())
			{
				PC->HandleShopOpenCommand(Payload);
			}
		});

	if (ARetrieveGameState* GS = World->GetGameState<ARetrieveGameState>())
	{
		const ERetrieveSessionState Current = GS->GetSessionState();
		HandleSessionStateChanged(Current, Current); // 최초 바인드는 전환이 아님
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

	if (ShopCommandHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			UGameplayMessageSubsystem::Get(World).UnregisterListener(ShopCommandHandle);
		}
		ShopCommandHandle = FGameplayMessageListenerHandle();
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

	CloseConversation();
	if (ShopFocusCameraActor)
	{
		ShopFocusCameraActor->Destroy();
		ShopFocusCameraActor = nullptr;
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

	// 라디얼 퀵슬롯 휠: 키를 누르고 있는 동안 열고, 떼면 방향 슬롯 사용
	if (QuickSlotWheelKey.IsValid() && Params.Key == QuickSlotWheelKey)
	{
		if (Params.Event == IE_Pressed)
		{
			// 다른 패널이 열려 있지 않을 때만 인게임 휠을 연다
			if (!ActivePanel && !bQuickSlotWheelOpen)
			{
				OpenQuickSlotWheel();
				return true;
			}
		}
		else if (Params.Event == IE_Released)
		{
			if (bQuickSlotWheelOpen)
			{
				CloseQuickSlotWheelAndUse();
				return true;
			}
		}
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

		if (SettingsPanelKey.IsValid() && Params.Key == SettingsPanelKey)
		{
			OpenSettingsPanel();
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

	// Possess된 폰 기준으로 Pawn/Camera 종속 설정(FOV)과 컨트롤러 설정(진동)을 적용한다.
	if (IsLocalController())
	{
		if (ULocalPlayer* LP = GetLocalPlayer())
		{
			if (URetrieveSettingsSubsystem* SettingsSubsystem = LP->GetSubsystem<URetrieveSettingsSubsystem>())
			{
				SettingsSubsystem->ApplyPawnSettings(InPawn);
				SettingsSubsystem->ApplyControllerSettings(this);
			}
		}
	}

	TryBindHealthToHUD();
	TryBindElementGaugeToHUD();
	if (AttackFeedbackComponent)
	{
		AttackFeedbackComponent->HandlePossessedPawnChanged(InPawn);
	}
}

void ARetrievePlayerController::RequestNewGame()
{
	Server_RequestNewGame();
}

void ARetrievePlayerController::HandleSessionStateChanged(ERetrieveSessionState Previous, ERetrieveSessionState NewState)
{
	RemoveActivePanelImmediately();
	CloseConversation();
	SwapActiveWidget(Previous, NewState);
	UpdateInputMode(NewState);
	HandleSessionPresentation(NewState);
}

void ARetrievePlayerController::SwapActiveWidget(ERetrieveSessionState Previous, ERetrieveSessionState NewState)
{
	if (ShouldShowLoadingCover(Previous, NewState))
	{
		ShowLoadingScreen();
	}
	
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
				if (!HUDViewModelInstance)
				{
					HUDViewModelInstance = NewObject<UHUDViewModel>(this);
				}
				TryBindHealthToHUD();
				TryBindElementGaugeToHUD();
			}
			ActiveTopLevelWidget->AddToViewport();
			if (NewState == ERetrieveSessionState::InGame)
			{
				EnsureHUDViewModel();
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

bool ARetrievePlayerController::ShouldShowLoadingCover(ERetrieveSessionState Previous,
                                                       ERetrieveSessionState NewState) const
{
	if (NewState != ERetrieveSessionState::InGame)
	{
		return false;
	}
	// 부활(Result→InGame): 항상 가림
	if (Previous == ERetrieveSessionState::Result)
	{
		return true;
	}
	// 새 게임/부팅 진입: dev 플래그로 생략 가능
	return !bSkipEntryLoadingScreen;
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

void ARetrievePlayerController::SetInputModeUIOnlyDuringConversation()
{
	FInputModeUIOnly Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
	bShowMouseCursor = true;
}

void ARetrievePlayerController::EnsureCinematicCloseListener()
{
	if (CinematicCloseHandle.IsValid())
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	CinematicCloseHandle = UGameplayMessageSubsystem::Get(World)
		.RegisterListener<FRetrieveCinematicStatePayload>(
			RetrieveGameplayTags::Channel_Cinematic_Changed,
			[WeakThis = TWeakObjectPtr<ARetrievePlayerController>(this)]
		(FGameplayTag /*Channel*/, const FRetrieveCinematicStatePayload& Message)
			{
				if (Message.bActive)
				{
					if (ARetrievePlayerController* PC = WeakThis.Get())
					{
						PC->CloseConversation();
					}
				}
			});
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

void ARetrievePlayerController::HandleSessionPresentation(ERetrieveSessionState NewState)
{
	switch (NewState)
	{
	case ERetrieveSessionState::MainMenu:
		ApplyMainMenuCamera();
		break;

	case ERetrieveSessionState::InGame:
		ApplyGameplayCamera();
		if (ActiveLoadingScreen)
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(LoadingScreenTimerHandle, this,
				                                  &ARetrievePlayerController::HideLoadingScreen,
				                                  LoadingScreenMinSeconds, false);
			}
			else
			{
				HideLoadingScreen();
			}
		}
		break;

	default:
		break;
	}
}

void ARetrievePlayerController::ApplyMainMenuCamera()
{
	if (APawn* MyPawn = GetPawn())
	{
		MyPawn->SetActorHiddenInGame(true);
		
		// TODO: (코옵) 권한 검사
		if (ACharacter* MyCharacter = Cast<ACharacter>(MyPawn))
		{
			if (UCharacterMovementComponent* MoveComp = MyCharacter->GetCharacterMovement())
			{
				MoveComp->StopMovementImmediately();
				MoveComp->DisableMovement();
			}
		}
	}

	if (AActor* CineCam = FindMainMenuCamera())
	{
		SetViewTargetWithBlend(CineCam, 0.f);
	}
}

void ARetrievePlayerController::ApplyGameplayCamera()
{
	if (APawn* MyPawn = GetPawn())
	{
		MyPawn->SetActorHiddenInGame(false);

		// TODO: (코옵) 권한 검사
		if (ACharacter* MyCharacter = Cast<ACharacter>(MyPawn))
		{
			if (UCharacterMovementComponent* MoveComp = MyCharacter->GetCharacterMovement())
			{
				MoveComp->SetDefaultMovementMode();
			}
		}
		
		// 메뉴 시점에서 플레이 시점으로 블렌드, 로딩 화면이 가림
		SetViewTargetWithBlend(MyPawn, MenuToGameplayBlendSeconds);
	}
}

AActor* ARetrievePlayerController::FindMainMenuCamera() const
{
	if (MainMenuCameraTag.IsNone())
	{
		return nullptr;
	}

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsWithTag(this, MainMenuCameraTag, Found);
	return Found.Num() > 0 ? Found[0] : nullptr;
}

void ARetrievePlayerController::ShowLoadingScreen()
{
	if (!LoadingScreenClass || ActiveLoadingScreen)
	{
		return;
	}

	ActiveLoadingScreen = CreateWidget<UUserWidget>(this, LoadingScreenClass);
	if (!ActiveLoadingScreen)
	{
		return;
	}

	// HUD (0), 토스트 (10), 패널 (50) 위의 ZOrder
	ActiveLoadingScreen->AddToViewport(100);

	// InGame에 도달하지 못했을 때 플레이어가 화면에 갇히지 않도록 함
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			LoadingScreenTimerHandle, this,
			&ARetrievePlayerController::HideLoadingScreen,
			LoadingScreenSafetySeconds, false);
	}
}

void ARetrievePlayerController::HideLoadingScreen()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
	}

	if (ActiveLoadingScreen)
	{
		if (URetrieveLoadingScreenWidget* LS = Cast<URetrieveLoadingScreenWidget>(ActiveLoadingScreen))
		{
			LS->PlayFadeOutAndRemove();
		}
		else
		{
			ActiveLoadingScreen->RemoveFromParent();
		}
		ActiveLoadingScreen = nullptr;
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
	if (UShopPanelWidget* ShopPanel = Cast<UShopPanelWidget>(NewPanel))
	{
		if (PendingShopDefinition)
		{
			ShopPanel->InitializeShopPanel(GetPawnInventoryComponent(), PendingShopDefinition, PendingShopComponent);
			if (bPendingShopOpenSellTab)
			{
				ShopPanel->SwitchToSellTab();
			}
			else
			{
				ShopPanel->SwitchToBuyTab();
			}
			PendingShopDefinition = nullptr;
			PendingShopComponent = nullptr;
			bPendingShopOpenSellTab = false;
		}
	}

	ActivePanel = NewPanel;
	ActivePanelClass = PanelClass;

	NewPanel->AddToViewport(PanelZOrder);
	CenterActiveWorldMapPanel();

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(NewPanel->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	bShowMouseCursor = true;

	NewPanel->SetKeyboardFocus();
	NewPanel->PlayPanelOpenVFX();
}

void ARetrievePlayerController::OpenSettingsPanel()
{
	TSubclassOf<URetrieveGamePanelWidget> PanelClass = SettingsPanelClass.LoadSynchronous();
	if (!PanelClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to open settings: SettingsPanelClass is empty."));
		return;
	}

	FRetrievePanelShortcutConfig SettingsShortcut;
	SettingsShortcut.Key = SettingsPanelKey;
	SettingsShortcut.PanelClass = PanelClass;
	if (CanOpenPanel(SettingsShortcut))
	{
		OpenExclusivePanel(PanelClass, SettingsPanelKey);
	}
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
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ActivePanelCloseFallbackTimerHandle,
			this,
			&ThisClass::HandleActivePanelCloseFallback,
			1.0f,
			false);
	}
}

void ARetrievePlayerController::RemoveActivePanelImmediately()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ActivePanelCloseFallbackTimerHandle);
	}

	if (!ActivePanel)
	{
		bActivePanelClosing = false;
		return;
	}

	ActivePanel->OnCloseRequested.RemoveDynamic(this, &ThisClass::HandleActivePanelCloseRequested);
	ActivePanel->OnUIVFXFinished.RemoveDynamic(this, &ThisClass::HandleActivePanelCloseVFXFinished);
	const bool bRemovingShopPanel = ActivePanel->IsA<UShopPanelWidget>();
	ActivePanel->RemoveFromParent();
	ActivePanel = nullptr;
	ActivePanelClass = nullptr;
	bActivePanelClosing = false;

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	bShowMouseCursor = false;

	// 상점 등으로 NPC를 비추던 카메라를 플레이어로 복귀한다.
	if (bShopCameraActive)
	{
		RestorePlayerCameraView();
	}

	if (bRemovingShopPanel)
	{
		bCanReturnToShopConversation = false;
		CurrentShopNPC = nullptr;
	}
}

void ARetrievePlayerController::FocusCameraOnActor(AActor* TargetActor)
{
	UWorld* World = GetWorld();
	if (!TargetActor || !World)
	{
		return;
	}

	// NPC 정면 기준으로 카메라 위치/회전 계산.
	// 메시 정면이 액터 Forward와 반대인 경우가 많아 OrbitYaw로 배치 방향을 조정한다.
	const FVector NpcLoc = TargetActor->GetActorLocation();
	const FVector Forward = TargetActor->GetActorForwardVector();
	const FVector Dir = Forward.RotateAngleAxis(ShopCameraOrbitYaw, FVector::UpVector);
	const FVector CamLoc = NpcLoc + Dir * ShopCameraDistance
		+ FVector(0.0f, 0.0f, ShopCameraHeight);
	const FVector BaseLookAt = NpcLoc + FVector(0.0f, 0.0f, ShopCameraLookAtHeight);
	const FRotator BaseCamRot = (BaseLookAt - CamLoc).Rotation();
	const FVector CameraRight = FRotationMatrix(BaseCamRot).GetUnitAxis(EAxis::Y);
	const FVector LookAt = BaseLookAt - CameraRight * ShopCameraFrameRightOffset;
	const FRotator CamRot = (LookAt - CamLoc).Rotation();

	UE_LOG(LogTemp, Warning,
		TEXT("[ShopCamera] PC=%s Target=%s Distance=%.1f Height=%.1f LookAtHeight=%.1f FOV=%.1f FrameRightOffset=%.1f OrbitYaw=%.1f CamLoc=%s LookAt=%s"),
		*GetClass()->GetPathName(),
		*GetNameSafe(TargetActor),
		ShopCameraDistance,
		ShopCameraHeight,
		ShopCameraLookAtHeight,
		ShopCameraFOV,
		ShopCameraFrameRightOffset,
		ShopCameraOrbitYaw,
		*CamLoc.ToCompactString(),
		*LookAt.ToCompactString());

	// 카메라 액터를 한 번만 스폰해 재사용한다.
	if (!ShopFocusCameraActor)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ShopFocusCameraActor = World->SpawnActor<ACameraActor>(
			ACameraActor::StaticClass(), CamLoc, CamRot, SpawnParams);
	}
	else
	{
		ShopFocusCameraActor->SetActorLocationAndRotation(CamLoc, CamRot);
	}

	if (!ShopFocusCameraActor)
	{
		return;
	}
	if (UCameraComponent* CameraComponent = ShopFocusCameraActor->GetCameraComponent())
	{
		CameraComponent->SetFieldOfView(ShopCameraFOV);
	}

	SetViewTargetWithBlend(ShopFocusCameraActor, ShopCameraBlendTime,
		EViewTargetBlendFunction::VTBlend_Cubic, 0.0f, false);
	bShopCameraActive = true;

	// 상점 카메라 동안 플레이어 폰을 일시적으로 숨긴다.
	if (bHidePlayerDuringShopCamera)
	{
		if (APawn* ControlledPawn = GetPawn())
		{
			ControlledPawn->SetActorHiddenInGame(true);
		}
	}
}

void ARetrievePlayerController::RestorePlayerCameraView()
{
	bShopCameraActive = false;

	if (AActor* PawnTarget = GetPawn())
	{
		// 숨겼던 플레이어 폰을 다시 표시한다.
		PawnTarget->SetActorHiddenInGame(false);

		SetViewTargetWithBlend(PawnTarget, ShopCameraBlendTime,
			EViewTargetBlendFunction::VTBlend_Cubic, 0.0f, false);
	}
}

void ARetrievePlayerController::HandleActivePanelCloseFallback()
{
	if (bActivePanelClosing)
	{
		RemoveActivePanelImmediately();
	}
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

void ARetrievePlayerController::OpenQuickSlotWheel()
{
	if (bQuickSlotWheelOpen || !QuickSlotWheelClass)
	{
		return;
	}

	if (!QuickSlotWheelInstance)
	{
		QuickSlotWheelInstance = CreateWidget<URetrieveQuickSlotWheelWidget>(this, QuickSlotWheelClass);
		if (QuickSlotWheelInstance)
		{
			QuickSlotWheelInstance->AddToViewport(70);
		}
	}

	if (!QuickSlotWheelInstance)
	{
		return;
	}

	QuickSlotWheelInstance->InitializeQuickSlotWheel(GetPawnInventoryComponent(), QuickSlotIconTable);
	QuickSlotWheelInstance->OpenForUse();

	// 마우스 방향 선택을 위해 GameAndUI. 커서는 추적은 유지하되 모양을 None으로 해서
	// 화면에는 보이지 않게 한다(절대 좌표 기반 방향 선택이 계속 동작).
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	bShowMouseCursor = true;
	CurrentMouseCursor = EMouseCursor::None;

	// 커서를 화면 중앙(휠 중심 부근)으로 옮겨 중립 상태에서 시작
	if (GEngine && GEngine->GameViewport)
	{
		FVector2D ViewportSize;
		GEngine->GameViewport->GetViewportSize(ViewportSize);
		SetMouseLocation(static_cast<int32>(ViewportSize.X * 0.5f),
		                 static_cast<int32>(ViewportSize.Y * 0.5f));
	}

	bQuickSlotWheelOpen = true;
}

void ARetrievePlayerController::CloseQuickSlotWheelAndUse()
{
	if (!bQuickSlotWheelOpen)
	{
		return;
	}

	if (QuickSlotWheelInstance)
	{
		// 마우스 방향으로 하이라이트된 슬롯 사용 후 휠 닫기
		QuickSlotWheelInstance->ActivateHighlightedSlotAndClose();
	}

	// 게임 입력 모드로 복귀 + 커서 모양 복원
	FInputModeGameOnly GameMode;
	SetInputMode(GameMode);
	bShowMouseCursor = false;
	CurrentMouseCursor = EMouseCursor::Default;

	bQuickSlotWheelOpen = false;
}

bool ARetrievePlayerController::CanOpenPanel(const FRetrievePanelShortcutConfig& ShortcutConfig) const
{
	if (!ShortcutConfig.PanelClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid panel shortcut: %s has no PanelClass."), *ShortcutConfig.Key.ToString());
		return false;
	}
	
	// 시네마틱 중에는 패널 차단
	if (const UWorld* World = GetWorld())
	{
		if (const ARetrieveGameState* GS = World->GetGameState<ARetrieveGameState>())
		{
			if (GS->GetCinematicState().IsActive())
			{
				return false;
			}
		}
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

void ARetrievePlayerController::Client_OpenConversation_Implementation(AActor* NPC)
{
	UWorld* World = GetWorld();
	ARetrieveGameState* GS = World ? World->GetGameState<ARetrieveGameState>() : nullptr;
	if (GS && GS->GetCinematicState().IsActive())
	{
		return;
	}

	if (!ConversationWidgetClass)
	{
		return;
	}

	RemoveActivePanelImmediately();

	if (!ConversationInstance)
	{
		ConversationInstance = CreateWidget<UUserWidget>(this, ConversationWidgetClass);
	}

	if (!ConversationInstance)
	{
		return;
	}

	if (ConversationVM)
	{
		ConversationVM->Deinitialize();
	}
	ConversationVM = NewObject<UConversationViewModel>(this);
	ConversationVM->Initialize(World, this);
	CurrentDialogueNPC = NPC;
	ConversationVM->BuildOpeningTopicsFor(NPC);

	// NPC 대화 시작 애니메이션 트리거
	if (NPC)
	{
		if (URetrieveDialogueComponent* DC = NPC->FindComponentByClass<URetrieveDialogueComponent>())
		{
			DC->PlayGreetingAnimation();
		}
	}

	if (UMVVMSubsystem* MVVM = GEngine ? GEngine->GetEngineSubsystem<UMVVMSubsystem>() : nullptr)
	{
		if (UMVVMView* View = MVVM->GetViewFromUserWidget(ConversationInstance))
		{
			View->SetViewModel(ConversationViewModelBindingName, ConversationVM);
		}
	}
	
	if (!ConversationInstance->IsInViewport())
	{
		ConversationInstance->AddToViewport(20);
	}
	SetInputModeUIOnlyDuringConversation();
	EnsureCinematicCloseListener();
	
	FRetrieveDialogueChangedPayload Payload;
	Payload.bActive = true;
	UGameplayMessageSubsystem::Get(this).BroadcastMessage(RetrieveGameplayTags::Channel_UI_DialogueChanged, Payload);
}

void ARetrievePlayerController::Server_RequestDialogueAdvance_Implementation(FGameplayTag TopicId)
{
	if (UWorld* World = GetWorld())
	{
		if (ARetrieveGameState* GS = World->GetGameState<ARetrieveGameState>())
		{
			GS->AdvanceDialogue(TopicId, GetPawn());
		}
	}

	// 선택된 TopicId에 해당하는 NPC 애니메이션 트리거
	if (CurrentDialogueNPC)
	{
		if (URetrieveDialogueComponent* DC = CurrentDialogueNPC->FindComponentByClass<URetrieveDialogueComponent>())
		{
			DC->PlayTopicAnimation(TopicId);
		}
	}
}

void ARetrievePlayerController::Server_RequestLumenToggleWait_Implementation()
{
}

void ARetrievePlayerController::CloseConversation()
{
	// NPC 유휴 애니메이션 복귀
	if (CurrentDialogueNPC)
	{
		if (URetrieveDialogueComponent* DC = CurrentDialogueNPC->FindComponentByClass<URetrieveDialogueComponent>())
		{
			DC->ReturnToIdle();
		}
	}
	CurrentDialogueNPC = nullptr;

	if (ConversationInstance)
	{
		ConversationInstance->RemoveFromParent();
		ConversationInstance = nullptr;

		if (ConversationVM)
		{
			ConversationVM->Deinitialize();
			ConversationVM = nullptr;
		}

		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
		bShowMouseCursor = false;
	}

	if (CinematicCloseHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			UGameplayMessageSubsystem::Get(World).UnregisterListener(CinematicCloseHandle);
		}
		CinematicCloseHandle = FGameplayMessageListenerHandle();
	}
	
	FRetrieveDialogueChangedPayload Payload;
	Payload.bActive = false;
	UGameplayMessageSubsystem::Get(this).BroadcastMessage(RetrieveGameplayTags::Channel_UI_DialogueChanged, Payload);
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
	View->SetViewModel(TEXT("PlayerStatus"), HUDViewModelInstance->GetPlayerStatus());
	View->SetViewModel(TEXT("ElementGauge"), HUDViewModelInstance->GetElementGauge());
	View->SetViewModel(TEXT("BossStatus"), HUDViewModelInstance->GetBossStatus());

	// QuestTracker는 URetrieveQuestTrackerWidget이 자체 NativeConstruct에서 VM 주입 + 시드를
	// 직접 처리한다 (WBP_HUD에 루트 MVVM 뷰가 없어 상단 주입이 닿지 않으므로 ElementGauge 방식).

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

		if (UQuestTrackerViewModel* Tracker = HUDViewModelInstance->GetQuestTracker())
		{
			Tracker->Deinitialize();
		}
	}
}

void ARetrievePlayerController::OpenShopPanel(TSubclassOf<URetrieveGamePanelWidget> ShopPanelClass,
                                               URetrieveShopDefinitionAsset* ShopDefinition)
{
	PendingShopDefinition = ShopDefinition;
	OpenExclusivePanel(ShopPanelClass, EKeys::Invalid);
}

void ARetrievePlayerController::HandleShopOpenCommand(const FRetrieveLumenCommandPayload& Payload)
{
	const bool bOpenSellTab = Payload.CommandTag.MatchesTagExact(RetrieveGameplayTags::Topic_ShopNPC_Sell)
		|| Payload.CommandTag.ToString().Contains(TEXT(".Sell"));
	OpenShopFromCurrentConversation(bOpenSellTab);
}

void ARetrievePlayerController::OpenShopFromCurrentConversation(bool bOpenSellTab)
{
	if (!ShopPanelWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("HandleShopOpenCommand: ShopPanelWidgetClass가 비어 있습니다. BP_RetrievePlayerController에서 할당하세요."));
		return;
	}

	URetrieveShopDefinitionAsset* ShopDef = nullptr;
	URetrieveShopComponent* ShopComp = nullptr;
	if (CurrentDialogueNPC)
	{
		ShopComp = CurrentDialogueNPC->FindComponentByClass<URetrieveShopComponent>();
		if (ShopComp)
			ShopDef = ShopComp->ShopDefinition;
	}

	if (!ShopDef)
	{
		UE_LOG(LogTemp, Warning, TEXT("HandleShopOpenCommand: NPC에 RetrieveShopComponent 또는 ShopDefinition이 없습니다."));
		return;
	}

	PendingShopComponent = ShopComp;
	bPendingShopOpenSellTab = bOpenSellTab;

	// CloseConversation()이 CurrentDialogueNPC를 비우므로 미리 보관한다.
	AActor* ShopNPC = CurrentDialogueNPC;
	CurrentShopNPC = ShopNPC;
	bCanReturnToShopConversation = true;

	CloseConversation();
	OpenShopPanel(ShopPanelWidgetClass, ShopDef);

	// 상점 NPC를 정면으로 비추도록 카메라를 블렌드한다.
	FocusCameraOnActor(ShopNPC);
}

bool ARetrievePlayerController::ReturnToShopConversation()
{
	if (!bCanReturnToShopConversation)
	{
		return false;
	}

	AActor* ShopNPC = CurrentShopNPC.Get();
	if (!ShopNPC)
	{
		return false;
	}

	RemoveActivePanelImmediately();
	Client_OpenConversation(ShopNPC);
	return true;
}
