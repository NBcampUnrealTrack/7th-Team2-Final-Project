#include "Player/RetrievePlayerController.h"

#include "Diagnostics/RetrieveDiagLog.h"
#include "MVVMSubsystem.h"
#include "Components/PanelWidget.h"
#include "Settings/RetrieveGameUserSettings.h"
#include "Settings/RetrieveSettingsSubsystem.h"
#include "UI/RetrieveSystemMessageWidget.h"
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
#include "Shop/RetrieveShopDefinitionAsset.h"
#include "Character/RetrieveAlsCharacter.h"
#include "Components/World/RetrieveDialogueComponent.h"
#include "Components/World/RetrieveShopComponent.h"
#include "Quest/QuestBranchComponent.h"
#include "UI/Map/RetrieveMinimapWidget.h"
#include "UI/Map/RetrieveWorldMapWidget.h"
#include "UI/RetrieveGamePanelWidget.h"
#include "UI/RetrieveUISettingsLibrary.h"
#include "UI/RetrieveSettingsPanelWidget.h"
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
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Subsystems/RetrieveCinematicSubsystem.h"
#include "UI/Loading/RetrieveLoadingScreenWidget.h"
#include "View/MVVMView.h"
#include "UObject/UnrealType.h"
#include "EngineUtils.h"

namespace
{
	/** owner에서 이름이 "InteractionTarget"인 컴포넌트를 찾아 InteractionEnabled를 reflection으로 토글한다.
	 *  상점 UI를 닫을 때 URetrieveShopComponent::OpenShop()에서 꺼둔 상호작용 프롬프트를 복원하는 데 사용된다. */
	void SetInteractionTargetEnabled(AActor* Owner, bool bEnabled)
	{
		if (!Owner)
		{
			return;
		}

		TArray<UActorComponent*> Comps;
		Owner->GetComponents(Comps);
		for (UActorComponent* Comp : Comps)
		{
			if (Comp && Comp->GetFName() == TEXT("InteractionTarget"))
			{
				if (FBoolProperty* EnabledProp =
					FindFProperty<FBoolProperty>(Comp->GetClass(), TEXT("InteractionEnabled")))
				{
					EnabledProp->SetPropertyValue_InContainer(Comp, bEnabled);
				}
				break;
			}
		}
	}

	/** 이 PlayerController에 붙은 InteractionManager 플러그인의 Manager_Interactor 컴포넌트를 찾아
	 *  OwnerController를 미리(this) 채워 넣는다.
	 *
	 *  플러그인은 OwnerController를 컴포넌트 BeginPlay(Construct_Player_Essentials)에서야 캐싱하고
	 *  복제하지 않는다. 코옵/조인 클라이언트에서는 서버의 Client_Update_PointOfInterests RPC가 그
	 *  BeginPlay보다 먼저 도착할 수 있어, On_PointOfInterest_Updated_ClientSide가 아직 None인
	 *  OwnerController를 읽고 "Accessed None"으로 죽는 초기화 순서 레이스가 발생한다.
	 *  BeginPlay/RPC 처리보다 앞서는 PostInitializeComponents 시점에 리플렉션으로 선캐싱해 창을 없앤다.
	 *  (컴포넌트 BeginPlay가 나중에 같은 값 this로 다시 세팅해도 무해하다.) */
	void PrimeInteractorOwnerController(APlayerController* PC)
	{
		if (!PC)
		{
			return;
		}

		TInlineComponentArray<UActorComponent*> Components(PC);
		for (UActorComponent* Comp : Components)
		{
			// PC에는 Manager_Interactor_C만 존재한다(월드 액터의 Manager_InteractionTarget과 이름이 겹치지 않음).
			if (!Comp || !Comp->GetClass()->GetName().Contains(TEXT("Manager_Interactor")))
			{
				continue;
			}

			if (FObjectProperty* OwnerProp =
				FindFProperty<FObjectProperty>(Comp->GetClass(), TEXT("OwnerController")))
			{
				OwnerProp->SetObjectPropertyValue_InContainer(Comp, PC);
			}
			break;
		}
	}

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
	LoadGamePanelClass = TSoftClassPtr<URetrieveGamePanelWidget>(
		FSoftObjectPath(TEXT("/Game/Retrieve/UI/Menu/WBP_LoadGame.WBP_LoadGame_C")));
	CreditsPanelClass = TSoftClassPtr<URetrieveGamePanelWidget>(
		FSoftObjectPath(TEXT("/Game/Retrieve/UI/Menu/WBP_Credits.WBP_Credits_C")));
	SystemMenuClass = TSoftClassPtr<URetrieveGamePanelWidget>(
		FSoftObjectPath(TEXT("/Game/Retrieve/UI/Menu/WBP_SystemMenu.WBP_SystemMenu_C")));
	ControlsGuideClass = TSoftClassPtr<URetrieveGamePanelWidget>(
		FSoftObjectPath(TEXT("/Game/Retrieve/UI/Menu/WBP_ControlsGuide.WBP_ControlsGuide_C")));
	SkillOverviewPanelClass = TSoftClassPtr<URetrieveGamePanelWidget>(
		FSoftObjectPath(TEXT("/Game/Retrieve/UI/Skill/WBP_SkillOverviewPanel.WBP_SkillOverviewPanel_C")));
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

void ARetrievePlayerController::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// InteractionManager(Manager_Interactor)의 OwnerController를 BeginPlay/RPC 처리 이전에 선캐싱.
	// (초기화 순서 레이스 상세는 PrimeInteractorOwnerController 주석 참조)
	PrimeInteractorOwnerController(this);
}

void ARetrievePlayerController::BeginPlay()
{
	RetrieveDiagCheckpoint(TEXT("PlayerController::BeginPlay start"));
	Super::BeginPlay();

#if UE_BUILD_SHIPPING
	// Texture Streaming 비활성화 상태에서는 월드 진입 직후 전체 해상도 리소스가
	// 정착하는 과정이 화면에 노출될 수 있으므로 Shipping 진입 커버를 강제한다.
	bSkipEntryLoadingScreen = false;
	LoadingScreenMinSeconds = FMath::Max(LoadingScreenMinSeconds, 5.0f);
#endif

	if (!IsLocalController())
	{
		return;
	}

	// 시네마틱 상태 리스너를 항상 등록 (대화창 닫기 + HUD 일괄 숨김/복원).
	// 기존에는 대화창을 처음 열 때만 등록되어, 대화 이전에 재생되는 오프닝 컷씬에서는 동작하지 않았다.
	EnsureCinematicCloseListener();

	// 창(앱) 전환 복귀 시 입력 상태 재정립 — look 입력이 죽는 입력 모드 잔류 방어
	if (FSlateApplication::IsInitialized())
	{
		AppActivationHandle = FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(
			this, &ARetrievePlayerController::HandleAppActivationChanged);
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

			// "HUD 숨기기" 설정 토글 시 즉시 반영되도록 설정 변경을 구독한다.
			SettingsSubsystem->OnSettingChanged.AddUniqueDynamic(this, &ARetrievePlayerController::HandleSettingsChangedForHUD);
			HUDSettingsSubsystem = SettingsSubsystem;
		}

		// 설정 창을 열지 않아도 조작키 안내 위젯이 리바인드된 키를 읽을 수 있도록,
		// PlayerMappable/IMC 등록을 부팅 시점에 한 번 보장한다.
		URetrieveSettingsPanelWidget::EnsureInputMappingsRegistered(LP);
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

	// 시네마틱 시작/종료 시 HUD를 숨기고/복원한다(대화는 열고 닫는 지점에서 직접 갱신).
	CinematicHUDVisibilityHandle = UGameplayMessageSubsystem::Get(World).RegisterListener<FRetrieveCinematicStatePayload>(
		RetrieveGameplayTags::Channel_Cinematic_Changed,
		[WeakThis = TWeakObjectPtr<ARetrievePlayerController>(this)]
	(FGameplayTag /*Channel*/, const FRetrieveCinematicStatePayload& /*Payload*/)
		{
			if (ARetrievePlayerController* PC = WeakThis.Get())
			{
				PC->UpdateHUDNarrativeVisibility();
			}
		});

	if (ARetrieveGameState* GS = World->GetGameState<ARetrieveGameState>())
	{
		const ERetrieveSessionState Current = GS->GetSessionState();
		RetrieveDiagCheckpoint(TEXT("PlayerController::BeginPlay -> HandleSessionStateChanged (initial)"));
		HandleSessionStateChanged(Current, Current); // 최초 바인드는 전환이 아님
	}
	RetrieveDiagCheckpoint(TEXT("PlayerController::BeginPlay end"));
}

void ARetrievePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveActivePanelImmediately();

	if (AppActivationHandle.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(AppActivationHandle);
		AppActivationHandle.Reset();
	}

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

	if (CinematicCloseHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			UGameplayMessageSubsystem::Get(World).UnregisterListener(CinematicCloseHandle);
		}
		CinematicCloseHandle = FGameplayMessageListenerHandle();
	}

	if (CinematicHUDVisibilityHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			UGameplayMessageSubsystem::Get(World).UnregisterListener(CinematicHUDVisibilityHandle);
		}
		CinematicHUDVisibilityHandle = FGameplayMessageListenerHandle();
	}

	if (URetrieveSettingsSubsystem* SettingsSubsystem = HUDSettingsSubsystem.Get())
	{
		SettingsSubsystem->OnSettingChanged.RemoveDynamic(this, &ARetrievePlayerController::HandleSettingsChangedForHUD);
	}
	HUDSettingsSubsystem = nullptr;

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

	// 라디얼 퀵슬롯 휠은 EnhancedInput IA_QuickSlotWheel(HeroComponent에서 Started/Completed 바인딩)이
	// 소유한다. 여기서 raw 키를 가로채면 이중 입력이 되므로 처리하지 않는다.

	if (Params.Event == IE_Pressed)
	{
		// 시네마틱 스킵: Space/ESC 연타(확인 창 내 2회). 시네마틱 중엔 DisableInput이 걸려 있지만
		// InputKey는 뷰포트 raw 경로라 여기서 처리한다. 스킵 불가 컷씬이면 서브시스템이 false를
		// 반환해 입력을 소비하지 않는다(ESC의 시스템 메뉴는 시네마틱 중 기존 차단이 그대로 유효).
		if ((Params.Key == EKeys::SpaceBar || Params.Key == EKeys::Escape) && IsCinematicActive())
		{
			if (UWorld* World = GetWorld())
			{
				if (URetrieveCinematicSubsystem* Cinematic = World->GetSubsystem<URetrieveCinematicSubsystem>())
				{
					if (Cinematic->NotifySkipKeyPressed())
					{
						return true;
					}
				}
			}
		}

		if (ActivePanel && Params.Key == EKeys::Escape)
		{
			CloseActivePanel();
			return true;
		}

		// 순수 게임플레이(패널 없음·휠 없음) 중 ESC → 시스템 메뉴.
		if (!ActivePanel && !bQuickSlotWheelOpen && Params.Key == EKeys::Escape)
		{
			OpenSystemMenu();
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

		if (SkillOverviewPanelKey.IsValid() && Params.Key == SkillOverviewPanelKey)
		{
			if (!IsCinematicActive() && !SkillOverviewPanelClass.IsNull())
			{
				OpenExclusivePanel(SkillOverviewPanelClass.LoadSynchronous(), SkillOverviewPanelKey);
			}
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

void ARetrievePlayerController::SetDeveloperSkipIntroFlow(bool bEnabled)
{
	bSkipEntryLoadingScreen = bEnabled;
}

void ARetrievePlayerController::RequestNewGame()
{
	Server_RequestNewGame();
}

void ARetrievePlayerController::EnterModalMessageInput(UUserWidget* MessageWidget)
{
	FInputModeUIOnly Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	if (MessageWidget)
	{
		Mode.SetWidgetToFocus(MessageWidget->TakeWidget());
	}
	SetInputMode(Mode);
	bShowMouseCursor = false;

	// 이 잠금이 활성인 동안엔 창 전환 복귀(HandleAppActivationChanged)가 GameOnly로 강제 복원해
	// 위젯 포커스를 뺏어 Enter 해제를 못 하게 만드는 것을 막는 플래그. 잠금 자체는 위 UIOnly가 담당.
	bModalMessageInputActive = true;
}

void ARetrievePlayerController::ExitModalMessageInput()
{
	bModalMessageInputActive = false;

	// 로딩 커버가 떠 있으면 UIOnly가 유지됨
	if (const ARetrieveGameState* GS = GetWorld() ? GetWorld()->GetGameState<ARetrieveGameState>() : nullptr)
	{
		UpdateInputMode(GS->GetSessionState());
	}
}

void ARetrievePlayerController::RequestContinueGame()
{
	Server_RequestContinueGame();
}

void ARetrievePlayerController::RequestLoadGameSlot(int32 SlotIndex)
{
	Server_RequestLoadGameSlot(SlotIndex);
}

void ARetrievePlayerController::RequestUnstuck()
{
	Server_RequestUnstuck();
}

void ARetrievePlayerController::RequestQuitGame()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, /*bIgnorePlatformRestrictions*/ false);
}

void ARetrievePlayerController::HandleAppActivationChanged(bool bIsActive)
{
	if (!bIsActive || !IsLocalController())
	{
		return;
	}

	// UI 컨텍스트(대화/패널/퀵슬롯 휠/로딩 커버/시네마틱/비 InGame)가 활성이면 각자의 로직이
	// 입력 모드를 관리하므로 개입하지 않는다. 순수 게임플레이 복귀에만 입력 상태를 재정립.
	const UWorld* World = GetWorld();
	const ARetrieveGameState* GS = World ? World->GetGameState<ARetrieveGameState>() : nullptr;
	const bool bPureGameplay = GS
		&& GS->GetSessionState() == ERetrieveSessionState::InGame
		&& !bLoadingCoverActive
		&& !IsCinematicActive()
		&& !ActivePanel
		&& !bModalMessageInputActive
		&& !(ConversationInstance && ConversationInstance->IsInViewport())
		&& !(QuickSlotWheelInstance && QuickSlotWheelInstance->IsInViewport());

	if (!bPureGameplay)
	{
		return;
	}

	// 창 전환 중 유실/잔류된 입력 상태 정리 후 게임 입력 모드 재적용
	ResetIgnoreInputFlags();
	FInputModeGameOnly Mode;
	SetInputMode(Mode);
	SetShowMouseCursor(false);
}

void ARetrievePlayerController::FlushPressedKeys()
{
	Super::FlushPressedKeys();

	// 창 포커스 상실/입력 모드 전환으로 Sprint(Hold)의 Completed 이벤트가 유실되면
	// State.Player.Sprinting 태그가 잔존해 입력 없이 계속 달리는 모션이 고착된다. 명시적으로 정리.
	if (ARetrievePlayerState* RetrievePS = GetPlayerState<ARetrievePlayerState>())
	{
		if (URetrieveAbilitySystemComponent* RetrieveASC = RetrievePS->GetRetrieveAbilitySystemComponent())
		{
			RetrieveASC->SetLooseGameplayTagCount(RetrieveGameplayTags::State_Player_Sprinting, 0);
		}
	}
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
	RetrieveDiagCheckpoint(TEXT("PlayerController::SwapActiveWidget start"));

	// 이전 전환에서 예약된 지연 스왑이 남아 있으면 취소한다(낡은 상태로 교체되는 것 방지).
	UWorld* SwapWorld = GetWorld();
	if (SwapWorld)
	{
		SwapWorld->GetTimerManager().ClearTimer(CoverGatedWidgetSwapTimerHandle);
	}

	const bool bShouldCover = ShouldShowLoadingCover(Previous, NewState);
	if (bShouldCover)
	{
		ShowLoadingScreen();
	}
	RetrieveDiagCheckpoint(TEXT("PlayerController::SwapActiveWidget - loading cover done"));

	// 커버가 올라오는 중이면 완전히 불투명해진 뒤에 위젯을 교체한다.
	// 즉시 교체하면 반투명한 커버 너머로 메뉴가 사라지고 HUD가 나타나는 것이 그대로 보인다.
	const float CoverDelay = bShouldCover ? GetActiveCoverFadeInSeconds() : 0.f;
	if (CoverDelay > KINDA_SMALL_NUMBER && SwapWorld)
	{
		SwapWorld->GetTimerManager().SetTimer(
			CoverGatedWidgetSwapTimerHandle,
			FTimerDelegate::CreateUObject(this, &ARetrievePlayerController::PerformWidgetSwap, NewState),
			CoverDelay, /*bLoop*/ false);
		RetrieveDiagCheckpoint(TEXT("PlayerController::SwapActiveWidget - swap deferred under cover"));
		return;
	}

	PerformWidgetSwap(NewState);
}

void ARetrievePlayerController::PerformWidgetSwap(ERetrieveSessionState NewState)
{
	RetrieveDiagCheckpoint(TEXT("PlayerController::PerformWidgetSwap start"));

	if (ActiveTopLevelWidget)
	{
		ClearHUDViewModel();
		ActiveTopLevelWidget->RemoveFromParent();
		ActiveTopLevelWidget = nullptr;
	}

	const TSubclassOf<UUserWidget> WidgetClass = ResolveWidgetClass(NewState);
	RetrieveDiagCheckpoint(*FString::Printf(TEXT("PlayerController::SwapActiveWidget - resolved class %s"),
		WidgetClass ? *WidgetClass->GetName() : TEXT("None")));
	if (WidgetClass)
	{
		RetrieveDiagCheckpoint(TEXT("PlayerController::SwapActiveWidget - before CreateWidget"));
		ActiveTopLevelWidget = CreateWidget<UUserWidget>(this, WidgetClass);
		RetrieveDiagCheckpoint(TEXT("PlayerController::SwapActiveWidget - after CreateWidget"));
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
			RetrieveDiagCheckpoint(TEXT("PlayerController::SwapActiveWidget - before AddToViewport"));
			ActiveTopLevelWidget->AddToViewport();
			RetrieveDiagCheckpoint(TEXT("PlayerController::SwapActiveWidget - after AddToViewport"));
			if (NewState == ERetrieveSessionState::InGame)
			{
				EnsureHUDViewModel();

				// 새 HUD 위젯 기준으로 내러티브(대화/시네마틱) 숨김 상태를 재평가한다.
				// (오프닝 시네마틱 재생 중 HUD가 생성되는 경우 즉시 숨겨진다.)
				bHUDHiddenForNarrative = false;
				UpdateHUDNarrativeVisibility();
			}
		}
	}

	// ── 토스트 매니저: InGame 상태에서만 활성화 ──────────────────────
	// HUD(ActiveTopLevelWidget)와 독립적으로 관리된다.
	// ZOrder=60 으로 HUD(0)·패널(50) 위에 렌더링.
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
			ActiveToastManager->AddToViewport(60);
		}
	}

	// 시네마틱 재생 중에 위젯이 교체/생성된 경우(오프닝과 상태 전환 경합 등) 새 위젯도 숨김 대상에 포함
	if (IsCinematicActive())
	{
		SetHUDHiddenForCinematic(true);
	}
	// "HUD 숨기기" 설정이 켜져 있으면 새 HUD의 게임플레이 요소를 숨긴다(시스템 메시지는 유지).
	if (IsHideHUDEnabled())
	{
		SetHUDHiddenForSetting(true);
	}
	RetrieveDiagCheckpoint(TEXT("PlayerController::PerformWidgetSwap end"));
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
	// 새 게임/부팅 진입: dev 플래그로 생략 가능.
	// (오프닝 컷씬이 장전돼 있어도 커버는 유지한다 — 메뉴에서 바로 첫 컷으로 점프하는 것보다
	//  로딩을 거치는 쪽이 자연스럽다는 피드백. 인게임 중 재생되는 시네마틱은 세션 전환이 아니라
	//  애초에 커버와 무관하게 바로 재생된다.)
	return !bSkipEntryLoadingScreen;
}

void ARetrievePlayerController::UpdateInputMode(ERetrieveSessionState NewState)
{
	// 여기서부터는 세션 상태가 입력 모드를 결정하므로 모달 메시지 잠금은 더 이상 유효하지 않다.
	// (메시지 위젯이 Exit 호출 없이 파괴되는 세션 전환 경로에서 플래그 잔류 방지.
	//  모달이 계속 표시 중이면 위젯 쪽 재진입 시 EnterModalMessageInput가 다시 세팅한다.)
	bModalMessageInputActive = false;

	switch (NewState)
	{
	case ERetrieveSessionState::MainMenu:
	case ERetrieveSessionState::Result:
		{
			// 게임 입력(GameOnly) → UIOnly로 나가면 눌려있던 이동/스프린트의 Completed(릴리즈)가 유실되어
			// 리스폰(Result→InGame) 후 입력 없이 계속 전진하는 고착이 생긴다. 나가기 직전에 눌림 상태를 정리한다.
			FlushPressedKeys();

			FInputModeUIOnly Mode;
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			if (ActiveTopLevelWidget)
			{
				Mode.SetWidgetToFocus(ActiveTopLevelWidget->TakeWidget());
			}
			SetInputMode(Mode);
			bShowMouseCursor = true;
			break;
		}

	case ERetrieveSessionState::InGame:
		{
			if (bLoadingCoverActive)
			{
				FInputModeUIOnly Mode;
				Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
				SetInputMode(Mode);
				bShowMouseCursor = false;
			}
			else
			{
				FInputModeGameOnly Mode;
				SetInputMode(Mode);
				bShowMouseCursor = false;
			}
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
	// UIOnly 전환 전에 눌림 상태를 정리 — 달리면서 스토리 트리거/대화에 진입하면 이동·스프린트의
	// 릴리즈 이벤트가 유실되어 대화 종료 후에도 계속 달리는 모션이 고착되는 것을 방지 (FlushPressedKeys 오버라이드 참고)
	FlushPressedKeys();

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
				ARetrievePlayerController* PC = WeakThis.Get();
				if (!PC)
				{
					return;
				}

				if (Message.bActive)
				{
					PC->CloseConversation();
				}
				// 시네마틱 동안 HUD 루트/토스트 일괄 숨김, 종료 시 복원
				PC->SetHUDHiddenForCinematic(Message.bActive);
			});
}

bool ARetrievePlayerController::IsCinematicActive() const
{
	const UWorld* World = GetWorld();
	const ARetrieveGameState* GS = World ? World->GetGameState<ARetrieveGameState>() : nullptr;
	return GS && GS->GetCinematicState().IsActive();
}

void ARetrievePlayerController::SetHUDHiddenForCinematic(bool bHideForCinematic)
{
	UE_LOG(LogTemp, Log, TEXT("[RetrieveCinematic] SetHUDHiddenForCinematic(%d): HUD=%s Toast=%s"),
		bHideForCinematic ? 1 : 0,
		ActiveTopLevelWidget ? *ActiveTopLevelWidget->GetName() : TEXT("null"),
		ActiveToastManager ? *ActiveToastManager->GetName() : TEXT("null"));

	if (bHideForCinematic)
	{
		// 재진입 허용: 시네마틱 중 새로 생성/표시된 위젯(HUD 교체, 토스트 등)을 추가로 숨길 수 있도록
		// "이미 적용됨" 조기 반환을 두지 않는다. 이미 추적 중인 위젯은 아래 루프에서 중복 등록되지 않는다.
		UWorld* World = GetWorld();
		if (!World)
		{
			return;
		}

		// HUD 루트(WBP_HUD)뿐 아니라 퀵슬롯/토스트처럼 뷰포트에 직접 AddToViewport된 위젯이 여럿이므로
		// 최상위 위젯 전체를 열거해 일괄 숨긴다. 시네마틱 오버레이는 이 호출 이후에 추가되지만 방어적으로 제외.
		const URetrieveCinematicSubsystem* CinematicSubsystem = World->GetSubsystem<URetrieveCinematicSubsystem>();

		TArray<UUserWidget*> TopLevelWidgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, TopLevelWidgets, UUserWidget::StaticClass(), /*TopLevelOnly=*/true);
		for (UUserWidget* Widget : TopLevelWidgets)
		{
			if (!Widget || !Widget->IsInViewport())
			{
				continue;
			}
			// 로딩 커버는 자체 수명(페이드아웃)에 맡긴다. 포인터 비교만으로는 부족 —
			// HideLoadingScreen이 페이드아웃을 시작하며 ActiveLoadingScreen을 즉시 비우는데, 같은 콜스택의
			// RevealGate로 시네마틱이 시작되면 페이드아웃 중인 커버가 여기서 Collapse되고(틱 정지 = 페이드 정지,
			// OnRemoved 미발화) 시네마틱 종료 복원 때 화면에 되살아나는 문제가 있었다. 클래스 기준으로 제외한다.
			if (Widget == ActiveLoadingScreen || Widget->IsA<URetrieveLoadingScreenWidget>())
			{
				continue;
			}
			if (CinematicSubsystem && CinematicSubsystem->IsCinematicOverlayWidget(Widget))
			{
				continue;
			}
			const ESlateVisibility CurrentVisibility = Widget->GetVisibility();
			if (CurrentVisibility == ESlateVisibility::Collapsed || CurrentVisibility == ESlateVisibility::Hidden)
			{
				continue; // 원래 숨어 있던 위젯은 복원 대상에서 제외 (원상태 유지)
			}
			const bool bAlreadyTracked = CinematicHiddenWidgets.ContainsByPredicate(
				[Widget](const TPair<TWeakObjectPtr<UUserWidget>, ESlateVisibility>& Entry)
				{
					return Entry.Key.Get() == Widget;
				});
			if (bAlreadyTracked)
			{
				Widget->SetVisibility(ESlateVisibility::Collapsed); // 도중에 스스로 켜진 위젯은 재숨김만
				continue;
			}
			CinematicHiddenWidgets.Emplace(Widget, CurrentVisibility);
			Widget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	else
	{
		// 숨김을 적용했던 위젯에만 원래 가시성 복원. 도중 파괴된 위젯은 약참조가 걸러낸다.
		for (const TPair<TWeakObjectPtr<UUserWidget>, ESlateVisibility>& Entry : CinematicHiddenWidgets)
		{
			if (UUserWidget* Widget = Entry.Key.Get())
			{
				Widget->SetVisibility(Entry.Value);
			}
		}
		CinematicHiddenWidgets.Reset();
	}
}

bool ARetrievePlayerController::IsHideHUDEnabled() const
{
	// "확정된" 값을 따른다(설정 창에서 토글만 한 프리뷰 값이 아니라 Apply/Reset/취소로 확정된 값).
	// → 설정 창에서 켜자마자 HUD가 사라지지 않고 "적용" 시에만 반영된다.
	if (const URetrieveSettingsSubsystem* SettingsSubsystem = HUDSettingsSubsystem.Get())
	{
		return SettingsSubsystem->IsHideHUDApplied();
	}
	// 폴백: 서브시스템 미확보 시 저장값 직접 조회(부팅 초기 등).
	const URetrieveGameUserSettings* Settings = URetrieveGameUserSettings::Get();
	return Settings && Settings->bHideHUD;
}

void ARetrievePlayerController::RefreshHUDHiddenForSetting()
{
	// HUD 숨기기 설정은 인게임(InGame)에서만 적용한다.
	// (메인 메뉴/결과 화면에서는 그 화면 위젯 자체가 최상위이므로, 여기서 숨기면 화면이 통째로 사라진다.)
	const UWorld* World = GetWorld();
	const ARetrieveGameState* GS = World ? World->GetGameState<ARetrieveGameState>() : nullptr;
	const bool bInGame = GS && GS->GetSessionState() == ERetrieveSessionState::InGame;
	SetHUDHiddenForSetting(bInGame && IsHideHUDEnabled());
}

void ARetrievePlayerController::SetHUDHiddenForSetting(bool bHide)
{
	// HUD 루트(WBP_HUD)를 통째로 접지 않고, 루트 캔버스의 게임플레이 자식들만 접는다.
	// (루트를 접으면 그 자식인 시스템/튜토리얼 메시지가 포커스를 못 받아 Enter로 넘길 수 없다.)
	UPanelWidget* RootPanel = ActiveTopLevelWidget ? Cast<UPanelWidget>(ActiveTopLevelWidget->GetRootWidget()) : nullptr;

	if (bHide)
	{
		if (!RootPanel)
		{
			return;
		}
		const int32 ChildCount = RootPanel->GetChildrenCount();
		for (int32 Index = 0; Index < ChildCount; ++Index)
		{
			UWidget* Child = RootPanel->GetChildAt(Index);
			if (!Child)
			{
				continue;
			}
			// 시스템/튜토리얼 메시지는 모달 Enter 입력을 받아야 하므로 숨기지 않는다(부모=루트는 유지).
			if (Child->IsA<URetrieveSystemMessageWidget>())
			{
				continue;
			}
			const ESlateVisibility CurrentVisibility = Child->GetVisibility();
			if (CurrentVisibility == ESlateVisibility::Collapsed || CurrentVisibility == ESlateVisibility::Hidden)
			{
				continue; // 원래 숨어 있던 것(예: 미출현 보스바)은 복원 대상에서 제외
			}
			const bool bAlreadyTracked = SettingHiddenWidgets.ContainsByPredicate(
				[Child](const TPair<TWeakObjectPtr<UWidget>, ESlateVisibility>& Entry)
				{
					return Entry.Key.Get() == Child;
				});
			if (bAlreadyTracked)
			{
				Child->SetVisibility(ESlateVisibility::Collapsed);
				continue;
			}
			SettingHiddenWidgets.Emplace(Child, CurrentVisibility);
			Child->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	else
	{
		// 숨겼던 자식들만 원래 가시성으로 복원. 도중 파괴된 위젯은 약참조가 걸러낸다.
		for (const TPair<TWeakObjectPtr<UWidget>, ESlateVisibility>& Entry : SettingHiddenWidgets)
		{
			if (UWidget* Child = Entry.Key.Get())
			{
				Child->SetVisibility(Entry.Value);
			}
		}
		SettingHiddenWidgets.Reset();
	}
}

void ARetrievePlayerController::HandleSettingsChangedForHUD(ERetrieveSettingsCategory Category)
{
	// 접근성 개별 변경 또는 전체 적용(MAX)일 때만 HUD 숨김 상태를 재평가한다.
	if (Category == ERetrieveSettingsCategory::Accessibility || Category == ERetrieveSettingsCategory::MAX)
	{
		RefreshHUDHiddenForSetting();
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

void ARetrievePlayerController::HandleSessionPresentation(ERetrieveSessionState NewState)
{
	UWorld* World = GetWorld();

	// 상태가 바뀌면 이전 상태에서 예약해 둔 커버 지연 카메라 전환은 무효화한다.
	if (World)
	{
		World->GetTimerManager().ClearTimer(CoverGatedCameraTimerHandle);
	}

	switch (NewState)
	{
	case ERetrieveSessionState::MainMenu:
		ApplyMainMenuCamera();
		break;

	case ERetrieveSessionState::InGame:
	{
		// 커버가 완전히 불투명해진 뒤에 뷰 타깃을 옮김
		const float CoverDelay = GetActiveCoverFadeInSeconds();
		if (CoverDelay > KINDA_SMALL_NUMBER && World)
		{
			World->GetTimerManager().SetTimer(CoverGatedCameraTimerHandle, this,
			                                  &ARetrievePlayerController::ApplyGameplayCamera,
			                                  CoverDelay, false);
		}
		else
		{
			ApplyGameplayCamera();
		}

		if (ActiveLoadingScreen)
		{
			if (World)
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
		else
		{
			BroadcastRevealGate(false);
		}
		break;
	}

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

float ARetrievePlayerController::GetActiveCoverFadeInSeconds() const
{
	if (const URetrieveLoadingScreenWidget* LS = Cast<URetrieveLoadingScreenWidget>(ActiveLoadingScreen))
	{
		return LS->GetCoverFadeInSeconds();
	}
	return 0.f;
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

	// HUD (0), 패널 (50), 토스트 (60) 위의 ZOrder
	ActiveLoadingScreen->AddToViewport(100);
	bLoadingCoverActive = true;
	BroadcastRevealGate(true); // 토스트/메시지 억제

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
			LS->OnRemoved.AddDynamic(this, &ARetrievePlayerController::HandleLoadingScreenRemoved);
			LS->PlayFadeOutAndRemove();
			if (UWorld* FadeWorld = GetWorld())
			{
				FadeWorld->GetTimerManager().SetTimer(LoadingScreenTimerHandle, this,
					&ARetrievePlayerController::HandleLoadingScreenRemoved, LoadingScreenSafetySeconds, false);
			}
		}
		else
		{
			ActiveLoadingScreen->RemoveFromParent();
			HandleLoadingScreenRemoved();
		}
		ActiveLoadingScreen = nullptr;
	}
	else
	{
		HandleLoadingScreenRemoved();
	}
	
	BroadcastRevealGate(false); // 커버 사라짐: 버퍼링된 위젯 표시, 오프닝 타임라인 시작
}

void ARetrievePlayerController::HandleLoadingScreenRemoved()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LoadingScreenTimerHandle);
	}
	bLoadingCoverActive = false;
	if (const ARetrieveGameState* GS = GetWorld() ? GetWorld()->GetGameState<ARetrieveGameState>() : nullptr)
	{
		UpdateInputMode(GS->GetSessionState());
	}
}

void ARetrievePlayerController::BroadcastRevealGate(bool bBlocked)
{
	if (UWorld* World = GetWorld())
	{
		FRetrieveRevealGatePayload Message;
		Message.bBlocked = bBlocked;
		UGameplayMessageSubsystem::Get(World).BroadcastMessage(RetrieveGameplayTags::Channel_UI_RevealGate, Message);
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

	// 고대비 HUD가 켜져 있으면 새로 열린 패널에도 즉시 적용한다(꺼져 있으면 no-op).
	URetrieveUISettingsLibrary::ApplyHighContrastToTree(NewPanel);

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

void ARetrievePlayerController::OpenLoadGamePanel()
{
	TSubclassOf<URetrieveGamePanelWidget> PanelClass = LoadGamePanelClass.LoadSynchronous();
	if (!PanelClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to open load game: LoadGamePanelClass is empty."));
		return;
	}

	FRetrievePanelShortcutConfig LoadGameShortcut;
	LoadGameShortcut.Key = EKeys::Escape;
	LoadGameShortcut.PanelClass = PanelClass;
	if (CanOpenPanel(LoadGameShortcut))
	{
		OpenExclusivePanel(PanelClass, EKeys::Escape);
	}
}

void ARetrievePlayerController::OpenSystemMenu()
{
	// 이미 다른 패널이 열려 있으면(인벤/월드맵/설정 등) ESC는 그 패널을 닫는 용도이므로 무시.
	if (ActivePanel)
	{
		return;
	}

	// 시네마틱 중 차단. 여기서 Retry/Unstuck/QuitToMenu가 실행되면 재생 중 세션 전환·강제 사망이 발동된다.
	// (다른 패널과 달리 이 경로는 CanOpenPanel을 거치지 않는다.)
	if (IsCinematicActive())
	{
		return;
	}

	TSubclassOf<URetrieveGamePanelWidget> PanelClass = SystemMenuClass.LoadSynchronous();
	if (!PanelClass)
	{
		// SystemMenuClass가 비어 있어도(예: BP에 None 오버라이드가 남은 경우) 알려진 경로로 폴백한다.
		// 이 경로 에셋은 DirectoriesToAlwaysCook(/Game/Retrieve/UI/Menu)로 항상 쿡되므로 패키지에서도 로드된다.
		PanelClass = LoadClass<URetrieveGamePanelWidget>(
			nullptr, TEXT("/Game/Retrieve/UI/Menu/WBP_SystemMenu.WBP_SystemMenu_C"));
	}
	if (!PanelClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to open pause menu: SystemMenuClass is empty and fallback load failed."));
		return;
	}

	// ESC를 ToggleKey로 넘겨 다시 ESC로 닫히게 한다.
	// 멀티플레이를 고려해 게임을 실제로 멈추지 않는다(패널이 열려 있는 동안 전투 입력은 PlayerTick에서 클라이언트 로컬로 차단됨).
	OpenExclusivePanel(PanelClass, EKeys::Escape);
}

void ARetrievePlayerController::OpenControlsGuide()
{
	TSubclassOf<URetrieveGamePanelWidget> PanelClass = ControlsGuideClass.LoadSynchronous();
	if (!PanelClass)
	{
		// 값이 비어 있어도 알려진 경로로 폴백(에셋은 /Game/Retrieve/UI/Menu always-cook로 패키지 포함).
		PanelClass = LoadClass<URetrieveGamePanelWidget>(
			nullptr, TEXT("/Game/Retrieve/UI/Menu/WBP_ControlsGuide.WBP_ControlsGuide_C"));
	}
	if (!PanelClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to open controls guide: ControlsGuideClass is empty and fallback load failed."));
		return;
	}

	// 시스템 메뉴에서 호출되므로, OpenExclusivePanel이 현재 패널을 교체하고 ESC로 닫히게 한다.
	OpenExclusivePanel(PanelClass, EKeys::Escape);

	// 리바인드된 키가 다이어그램에 반영되도록 C++에서 직접 갱신한다(WBP 그래프 배선에 의존하지 않음).
	if (ActivePanel)
	{
		URetrieveUISettingsLibrary::RefreshControlsGuideKeyLabels(ActivePanel);
	}
}

void ARetrievePlayerController::OpenCreditsPanel()
{
	TSubclassOf<URetrieveGamePanelWidget> PanelClass = CreditsPanelClass.LoadSynchronous();
	if (!PanelClass)
	{
		// 값이 비어 있어도 알려진 경로로 폴백(에셋은 /Game/Retrieve/UI/Menu always-cook로 패키지 포함).
		PanelClass = LoadClass<URetrieveGamePanelWidget>(
			nullptr, TEXT("/Game/Retrieve/UI/Menu/WBP_Credits.WBP_Credits_C"));
	}
	if (!PanelClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to open credits: CreditsPanelClass is empty and fallback load failed."));
		return;
	}

	// 메인메뉴에서 호출되므로, OpenExclusivePanel이 현재 패널을 교체하고 ESC로 닫히게 한다.
	OpenExclusivePanel(PanelClass, EKeys::Escape);
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

	// 패널을 닫은 뒤 입력 모드를 현재 세션 상태에 맞게 복원한다.
	// 메인메뉴/결과 화면에서 설정 패널을 열었다 닫으면 UIOnly+커서를 유지해야 하고,
	// 인게임에서만 GameOnly로 돌아간다. (예전엔 무조건 GameOnly로 강제해서, 메인메뉴에서
	// 설정을 닫으면 커서와 UI 입력이 죽어 메뉴가 조작 불가 상태가 됐다.)
	ERetrieveSessionState SessionState = ERetrieveSessionState::InGame;
	if (const ARetrieveGameState* GS = GetWorld() ? GetWorld()->GetGameState<ARetrieveGameState>() : nullptr)
	{
		SessionState = GS->GetSessionState();
	}

	if (SessionState == ERetrieveSessionState::MainMenu || SessionState == ERetrieveSessionState::Result)
	{
		UpdateInputMode(SessionState);
	}
	else
	{
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
		bShowMouseCursor = false;
	}

	// 상점 등으로 NPC를 비추던 카메라를 플레이어로 복귀한다.
	if (bShopCameraActive)
	{
		RestorePlayerCameraView();
	}

	if (bRemovingShopPanel)
	{
		bCanReturnToShopConversation = false;
		SetInteractionTargetEnabled(CurrentShopNPC, true);
		CurrentShopNPC = nullptr;
	}
}

void ARetrievePlayerController::FocusCameraOnActor(AActor* TargetActor, TOptional<float> OrbitYawOverride,
	TOptional<float> DistanceOverride, TOptional<float> FOVOverride, TOptional<float> FrameRightOffsetOverride,
	TOptional<float> HeightOverride, TOptional<float> LookAtHeightOverride)
{
	UWorld* World = GetWorld();
	if (!TargetActor || !World)
	{
		return;
	}

	// NPC 정면 기준으로 카메라 위치/회전 계산.
	// 메시 정면이 액터 Forward와 반대인 경우가 많아 OrbitYaw로 배치 방향을 조정한다.
	// (캐릭터 종류마다 이 관계가 달라 호출부에서 오버라이드할 수 있다 — 상점 NPC는 180이 정면이지만
	// Villager는 메시 정면이 액터 Forward와 일치해 0이 정면이다.)
	const float OrbitYaw = OrbitYawOverride.IsSet() ? OrbitYawOverride.GetValue() : ShopCameraOrbitYaw;
	const float Distance = DistanceOverride.IsSet() ? DistanceOverride.GetValue() : ShopCameraDistance;
	const float FOV = FOVOverride.IsSet() ? FOVOverride.GetValue() : ShopCameraFOV;
	const float FrameRightOffset = FrameRightOffsetOverride.IsSet() ? FrameRightOffsetOverride.GetValue() : ShopCameraFrameRightOffset;
	const float Height = HeightOverride.IsSet() ? HeightOverride.GetValue() : ShopCameraHeight;
	const float LookAtHeight = LookAtHeightOverride.IsSet() ? LookAtHeightOverride.GetValue() : ShopCameraLookAtHeight;

	const FVector NpcLoc = TargetActor->GetActorLocation();
	const FVector Forward = TargetActor->GetActorForwardVector();
	const FVector Dir = Forward.RotateAngleAxis(OrbitYaw, FVector::UpVector);
	const FVector CamLoc = NpcLoc + Dir * Distance
		+ FVector(0.0f, 0.0f, Height);
	const FVector BaseLookAt = NpcLoc + FVector(0.0f, 0.0f, LookAtHeight);
	const FRotator BaseCamRot = (BaseLookAt - CamLoc).Rotation();
	const FVector CameraRight = FRotationMatrix(BaseCamRot).GetUnitAxis(EAxis::Y);
	const FVector LookAt = BaseLookAt - CameraRight * FrameRightOffset;
	const FRotator CamRot = (LookAt - CamLoc).Rotation();

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
		CameraComponent->SetFieldOfView(FOV);
	}

	SetViewTargetWithBlend(ShopFocusCameraActor, ShopCameraBlendTime,
		EViewTargetBlendFunction::VTBlend_Cubic, 0.0f, false);
	bShopCameraActive = true;

	// NPC 포커스 카메라가 활성화된 동안 다른 NPC들의 상호작용 프롬프트가 화면에 뜨지 않도록 숨긴다.
	HideAllInteractionTargetsForCamera();

	// 상점 카메라 동안 플레이어 폰을 일시적으로 숨긴다.
	if (bHidePlayerDuringShopCamera)
	{
		if (APawn* ControlledPawn = GetPawn())
		{
			ControlledPawn->SetActorHiddenInGame(true);
		}
	}
}

void ARetrievePlayerController::FocusDialogueTwoShotCamera(AActor* TargetActor)
{
	UWorld* World = GetWorld();
	APawn* PlayerPawn = GetPawn();
	if (!TargetActor || !World || !PlayerPawn)
	{
		return;
	}

	const FVector PlayerLoc = PlayerPawn->GetActorLocation();
	const FVector NpcLoc = TargetActor->GetActorLocation();

	// 플레이어→NPC 수평 축. 두 액터 위치만 사용하므로 메시 정면-액터 Forward 불일치와 무관하다.
	FVector Axis = NpcLoc - PlayerLoc;
	Axis.Z = 0.f;
	const float PairDistance = Axis.Size();
	const FVector AxisDir = PairDistance > 1.f ? Axis / PairDistance : FVector(PlayerPawn->GetActorForwardVector().GetSafeNormal2D());
	const FVector AxisRight = FVector::CrossProduct(FVector::UpVector, AxisDir); // 축 진행 방향의 오른쪽

	// 플레이어 어깨 뒤 + 축 오른쪽으로 비껴난 위치 → 플레이어는 좌측 전경, NPC는 우중앙(참조 구도).
	const FVector CamLoc = PlayerLoc
		- AxisDir * DialogueTwoShotBackDistance
		+ AxisRight * DialogueTwoShotSideOffset
		+ FVector(0.f, 0.f, DialogueTwoShotCameraHeight);

	// 시선은 플레이어와 NPC 사이(NPC 쪽으로 치우침) — 두 인물이 모두 프레임에 들어온다.
	const FVector LookAt = FMath::Lerp(PlayerLoc, NpcLoc, DialogueTwoShotLookAtBias)
		+ FVector(0.f, 0.f, DialogueTwoShotLookAtHeight);
	const FRotator CamRot = (LookAt - CamLoc).Rotation();

	// NPC 단독 샷과 같은 카메라 액터를 재사용한다.
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
		CameraComponent->SetFieldOfView(DialogueTwoShotFOV);
	}

	SetViewTargetWithBlend(ShopFocusCameraActor, ShopCameraBlendTime,
		EViewTargetBlendFunction::VTBlend_Cubic, 0.0f, false);
	bShopCameraActive = true;

	// 다른 NPC들의 상호작용 프롬프트가 화면에 뜨지 않도록 숨긴다(단독 샷과 동일).
	HideAllInteractionTargetsForCamera();

	// 투샷에서는 플레이어가 프레임의 절반이므로 절대 숨기지 않는다.
	if (APawn* ControlledPawn = GetPawn())
	{
		ControlledPawn->SetActorHiddenInGame(false);
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

	RestoreAllInteractionTargetsForCamera();
}

void ARetrievePlayerController::HideAllInteractionTargetsForCamera()
{
	if (bInteractionTargetsHiddenForCamera)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	HiddenInteractionActorsForCamera.Reset();

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		TArray<UActorComponent*> Comps;
		Actor->GetComponents(Comps);
		for (UActorComponent* Comp : Comps)
		{
			if (Comp && Comp->GetFName() == TEXT("InteractionTarget"))
			{
				if (FBoolProperty* EnabledProp =
					FindFProperty<FBoolProperty>(Comp->GetClass(), TEXT("InteractionEnabled")))
				{
					if (EnabledProp->GetPropertyValue_InContainer(Comp))
					{
						EnabledProp->SetPropertyValue_InContainer(Comp, false);
						HiddenInteractionActorsForCamera.Add(Actor);
					}
				}
				break;
			}
		}
	}

	bInteractionTargetsHiddenForCamera = true;
}

void ARetrievePlayerController::RestoreAllInteractionTargetsForCamera()
{
	if (!bInteractionTargetsHiddenForCamera)
	{
		return;
	}

	for (const TWeakObjectPtr<AActor>& WeakActor : HiddenInteractionActorsForCamera)
	{
		if (AActor* Actor = WeakActor.Get())
		{
			SetInteractionTargetEnabled(Actor, true);
		}
	}

	HiddenInteractionActorsForCamera.Reset();
	bInteractionTargetsHiddenForCamera = false;
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

		OpenExclusivePanel(ShortcutConfig.PanelClass.LoadSynchronous(), ShortcutConfig.Key);
		return true;
	}

	return false;
}

void ARetrievePlayerController::OpenQuickSlotWheel()
{
	// 다른 패널이 열려 있거나(ActivePanel), 이미 열려 있거나, 시네마틱 중이면 열지 않는다.
	// (예전엔 이 패널 가드가 InputKey 래퍼에 있었으나, EnhancedInput 경로로 옮기며 여기로 통합.)
	if (bQuickSlotWheelOpen || ActivePanel || !QuickSlotWheelClass || IsCinematicActive())
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
		if (IsCinematicActive())
		{
			// 휠을 연 채로 시네마틱이 시작된 경우: 아이템 사용 없이 닫기만 한다
			QuickSlotWheelInstance->CloseWheel();
		}
		else
		{
			// 마우스 방향으로 하이라이트된 슬롯 사용 후 휠 닫기
			QuickSlotWheelInstance->ActivateHighlightedSlotAndClose();
		}
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
	// TSoftClassPtr는 로드되기 전까지 operator bool()(=IsValid(), 즉 "현재 메모리에 로드돼 있는가")이
	// false를 반환한다. 아직 LoadSynchronous()를 호출하기 전인 이 시점에는 항상 false가 되어 모든
	// 패널이 열리지 않는 회귀가 발생했었다. "경로 자체가 비어있는가"를 뜻하는 IsNull()로 검사해야 한다.
	if (ShortcutConfig.PanelClass.IsNull())
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

void ARetrievePlayerController::Server_RequestContinueGame_Implementation()
{
	if (ARetrieveGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ARetrieveGameMode>() : nullptr)
	{
		GM->HandleContinueGame(this);
	}
}

void ARetrievePlayerController::Server_RequestLoadGameSlot_Implementation(int32 SlotIndex)
{
	if (ARetrieveGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ARetrieveGameMode>() : nullptr)
	{
		GM->HandleLoadGameSlot(this, SlotIndex);
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

void ARetrievePlayerController::Server_RequestUnstuck_Implementation()
{
	if (ARetrieveGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ARetrieveGameMode>() : nullptr)
	{
		GM->HandleUnstuck(this);
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

	// NPC 대화 시작 애니메이션 트리거 + 플레이어가 상대를 바라보게 회전
	if (NPC)
	{
		if (URetrieveDialogueComponent* DC = NPC->FindComponentByClass<URetrieveDialogueComponent>())
		{
			DC->PlayGreetingAnimation();
		}

		// 플레이어가 대화 상대 방향으로 부드럽게 회전 (ALS 일관 회전; 1° 이내 정렬 시 자동 종료)
		if (ARetrieveAlsCharacter* PlayerChar = Cast<ARetrieveAlsCharacter>(GetPawn()))
		{
			PlayerChar->TurnYawTowardActor(NPC, ConversationFaceInterpSpeed);
		}
	}

	// 상점 NPC와 동일하게, 대화 시작 시 카메라를 NPC 정면으로 블렌드한다.
	// Villager 계열은 메시 정면이 액터 Forward와 일치하므로(상점 NPC와 반대) OrbitYaw=0을 쓰고,
	// 상점 NPC(URetrieveShopComponent 보유)는 메시 정면이 반대이므로 OrbitYaw 오버라이드를 생략해
	// FocusCameraOnActor가 ShopCameraOrbitYaw(기본 180)로 폴백하게 둔다.
	// (이전엔 여기서 항상 0.0f를 넘겨 상점 NPC와의 대화 시작 화면이 후면으로 잡히는 회귀가 있었다.)
	if (bDialogueTwoShotCamera)
	{
		// 참조 구도: 플레이어 어깨 뒤 사선에서 두 인물이 함께 보이는 투샷.
		FocusDialogueTwoShotCamera(NPC);
	}
	else
	{
		const bool bIsShopNPC = NPC && NPC->FindComponentByClass<URetrieveShopComponent>() != nullptr;
		TOptional<float> DialogueOrbitYawOverride = bIsShopNPC ? TOptional<float>() : TOptional<float>(0.0f);
		// 상점보다 더 확대해 화면 좌측에 가깝게 배치하도록 Dialogue* 오버라이드를 넘긴다.
		FocusCameraOnActor(NPC, DialogueOrbitYawOverride, DialogueCameraDistance, DialogueCameraFOV, DialogueCameraFrameRightOffset,
			DialogueCameraHeight, DialogueCameraLookAtHeight);
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

	// 대화 중에는 HUD를 숨긴다(종료 시 CloseConversation에서 복원).
	UpdateHUDNarrativeVisibility();
}

void ARetrievePlayerController::Server_RequestDialogueAdvance_Implementation(FGameplayTag TopicId)
{
	// 가위바위보 내기 토픽(Dialogue.Bet.*)은 DT_Dialogue 행이 없으므로
	// 대화 중인 NPC의 DialogueComponent가 직접 처리한다.
	if (CurrentDialogueNPC)
	{
		if (URetrieveDialogueComponent* BetDC = CurrentDialogueNPC->FindComponentByClass<URetrieveDialogueComponent>())
		{
			if (BetDC->HandleRpsBetTopic(TopicId, GetPawn()))
			{
				return;
			}
		}
	}

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
	AActor* ClosedDialogueNPC = CurrentDialogueNPC;
	if (ClosedDialogueNPC)
	{
		Server_NotifyDialogueClosed(ClosedDialogueNPC);
	}

	// NPC 유휴 애니메이션 복귀
	if (CurrentDialogueNPC)
	{
		if (URetrieveDialogueComponent* DC = CurrentDialogueNPC->FindComponentByClass<URetrieveDialogueComponent>())
		{
			DC->ReturnToIdle();

			if (HasAuthority())
			{
				DC->TryGrantItemReward(GetPawn());
			}
		}
	}
	CurrentDialogueNPC = nullptr;

	// 대화 시작 시 NPC로 블렌드했던 카메라를 플레이어로 복귀한다.
	// (상점으로 전환되는 경우 OpenShopFromCurrentConversation이 곧바로 FocusCameraOnActor를
	// 다시 호출해 상점 NPC로 재전환하므로 여기서 복귀해도 무방하다.)
	if (bShopCameraActive)
	{
		RestorePlayerCameraView();
	}

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

	// 주의: CinematicCloseHandle은 여기서 해제하지 않는다. 리스너가 "대화창 열 때 등록"이던 시절의
	// 해제 코드가 남아 있으면, 대화창 정리 한 번에 상시 리스너(BeginPlay 등록)가 죽어서
	// 이후 시네마틱의 HUD 일괄 숨김/대화창 닫기가 통째로 동작하지 않는다. 해제는 EndPlay에서 수행한다.

	// 대화 종료: 시네마틱도 진행 중이 아니면 HUD를 복원한다.
	// (HUD 가시성을 먼저 되돌려야, 아래 DialogueChanged 브로드캐스트로 시스템 메시지 위젯이
	//  다시 표시될 때 부모 HUD가 이미 Visible이라 SetKeyboardFocus가 정상적으로 걸린다.
	//  순서가 반대면 접힌 부모 밑에서 포커스가 실패해 Enter로 메시지를 못 닫는다.)
	UpdateHUDNarrativeVisibility();

	FRetrieveDialogueChangedPayload Payload;
	Payload.bActive = false;
	UGameplayMessageSubsystem::Get(this).BroadcastMessage(RetrieveGameplayTags::Channel_UI_DialogueChanged, Payload);
}

void ARetrievePlayerController::Server_NotifyDialogueClosed_Implementation(AActor* NPC)
{
	APawn* PlayerPawn = GetPawn();
	if (!IsValid(NPC) || !IsValid(PlayerPawn))
	{
		return;
	}

	constexpr float MaxDialogueCloseDistance = 1200.0f;
	if (FVector::DistSquared(PlayerPawn->GetActorLocation(), NPC->GetActorLocation())
		> FMath::Square(MaxDialogueCloseDistance))
	{
		return;
	}

	if (URetrieveDialogueComponent* Dialogue = NPC->FindComponentByClass<URetrieveDialogueComponent>())
	{
		Dialogue->NotifyDialogueClosed(PlayerPawn);
	}
}

void ARetrievePlayerController::UpdateHUDNarrativeVisibility()
{
	if (!ActiveTopLevelWidget)
	{
		bHUDHiddenForNarrative = false;
		return;
	}

	UWorld* World = GetWorld();
	ARetrieveGameState* GS = World ? World->GetGameState<ARetrieveGameState>() : nullptr;
	// 인게임 HUD가 아닐 때(메인 메뉴/결과 화면)는 건드리지 않는다.
	if (!GS || GS->GetSessionState() != ERetrieveSessionState::InGame)
	{
		return;
	}

	const bool bCinematicActive = GS->GetCinematicState().IsActive();
	const bool bDialogueActive = ConversationInstance != nullptr;
	const bool bShouldHide = bCinematicActive || bDialogueActive;

	if (bShouldHide && !bHUDHiddenForNarrative)
	{
		SavedHUDVisibilityForNarrative = ActiveTopLevelWidget->GetVisibility();
		ActiveTopLevelWidget->SetVisibility(ESlateVisibility::Collapsed);
		bHUDHiddenForNarrative = true;
	}
	else if (!bShouldHide && bHUDHiddenForNarrative)
	{
		ActiveTopLevelWidget->SetVisibility(SavedHUDVisibilityForNarrative);
		bHUDHiddenForNarrative = false;
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

	// 자식 ViewModel도 명시적으로 등록 — WBP_HPBar처럼 클래스 직접 참조 방식과 호환
	View->SetViewModel(TEXT("PlayerStatus"), HUDViewModelInstance->GetPlayerStatus());
	View->SetViewModel(TEXT("ElementGauge"), HUDViewModelInstance->GetElementGauge());
	View->SetViewModel(TEXT("BossStatus"), HUDViewModelInstance->GetBossStatus());

	// 레티클은 URetrieveReticleWidget이 자체 NativeConstruct에서 VM 주입 + 소스 바인딩을
	// 직접 처리한다 (WBP_HUD에 루트 MVVM 뷰가 없어 상단 주입이 닿지 않으므로 QuestTracker 방식).

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

	// 상점 패널이 열려있는 동안 이 NPC의 상호작용 프롬프트를 숨긴다.
	// (실제 상점 진입 경로가 URetrieveShopComponent::OpenShop()이 아니라 대화 커맨드를 거치므로 여기서 처리)
	SetInteractionTargetEnabled(ShopNPC, false);

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


