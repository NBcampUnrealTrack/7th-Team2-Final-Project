#pragma once

#include "CoreMinimal.h"
#include "Components/SlateWrapperTypes.h"
#include "Core/RetrieveSessionState.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"
#include "RetrievePlayerController.generated.h"

class ARetrievePlayerState;
class ACameraActor;
class UInventoryComponent;
class URetrieveGamePanelWidget;
class URetrieveMinimapWidget;
class URetrieveHealthComponent;
class URetrieveBossHPBarWidget;
class URetrieveQuickSlotWheelWidget;
class URetrieveShopComponent;
class URetrieveShopDefinitionAsset;
class UUserWidget;
class UDataTable;
class UWeaponComponent;
class UHUDViewModel;
class UConversationViewModel;
class UAttackFeedbackComponent;
struct FRetrieveLumenCommandPayload;

USTRUCT(BlueprintType)
struct FRetrievePanelShortcutConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|UI")
	FKey Key;

	// 임시: WBP_InventoryPanel 로딩 시 Shipping에서만 재현되는 IoStore 익스포트 테이블 크래시를
	// 회피하기 위해 하드 레퍼런스(TSubclassOf) 대신 지연 로딩(TSoftClassPtr)으로 전환.
	// 부팅 시점이 아니라 실제로 패널을 열 때 로드되므로, 문제가 있는 패널을 열 때만 크래시가
	// 발생하고 나머지 게임플레이는 정상 진행된다. 근본 원인 확인 후 되돌릴 것.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|UI")
	TSoftClassPtr<URetrieveGamePanelWidget> PanelClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|UI")
	bool bRequiresInventoryOpenPermission = false;
};

UCLASS()
class RETRIEVE_API ARetrievePlayerController : public APlayerController
{
	GENERATED_BODY()

#pragma region Lifecycle & Overrides

public:
	ARetrievePlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	//PostInitializeComponents, BeginPlay, EndPlay, PlayerTick, InputKey, AcknowledgePossession
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;
	virtual void AcknowledgePossession(APawn* InPawn) override;

#pragma endregion

#pragma region Session & Widget Flow

public:
	ARetrievePlayerState* GetRetrievePlayerState() const;

	void SetDeveloperSkipIntroFlow(bool bEnabled);

	UFUNCTION(Server, Reliable)
	void Server_RequestNewGame();

	UFUNCTION(Server, Reliable)
	void Server_RequestRetry();

	UFUNCTION(Server, Reliable)
	void Server_RequestQuitToMenu();

	UFUNCTION(Server, Reliable)
	void Server_RequestContinueGame();

	UFUNCTION(Server, Reliable)
	void Server_RequestLoadGameSlot(int32 SlotIndex);

	UFUNCTION(Server, Reliable)
	void Server_RequestUnstuck();

	/**
	 * 메뉴 - 새 게임 진입점. 로컬에 코스메틱 로딩 화면을 표시한 후,
	 * 호스트 전용 Server_RequestNewGame (-> HandleNewGame -> MainMenu->InGame)을 호출합니다. W_MainMenu에서 호출.
	 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Menu")
	void RequestNewGame();
	
	/** 해제 필수 시스템 메시지 표시 중 입력 잠금 (UIOnly + 위젯 포커스, Enter로만 해제). */
	void EnterModalMessageInput(UUserWidget* MessageWidget);

	/** 해제 필수 큐가 비면 세션 상태 기준으로 입력 모드를 복구. */
	void ExitModalMessageInput();

	/**
	 * 메뉴 - 이어하기 진입점. 가장 최근 저장 슬롯을 서버가 판별해 로드합니다
	 * (-> HandleContinueGame -> SaveSubsystem::LoadFromSlot -> MainMenu->InGame). W_MainMenu에서 호출.
	 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Menu")
	void RequestContinueGame();

	/**
	 * 메뉴 - 불러오기(슬롯 선택) 진입점. 지정 슬롯을 서버가 로드합니다
	 * (-> HandleLoadGameSlot -> SaveSubsystem::LoadFromSlot -> MainMenu->InGame). WBP_LoadGame에서 호출.
	 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Menu")
	void RequestLoadGameSlot(int32 SlotIndex);

	/** 언스턱/강제 리스폰. 정규 사망 흐름(로딩 커버 포함)으로 마지막 체크포인트에 리스폰. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Menu")
	void RequestUnstuck();

	/** 게임 종료(애플리케이션 종료). 포즈 팝업에서 호출. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Menu")
	void RequestQuitGame();

	/** 창 포커스 상실/입력 모드 전환 시 엔진이 호출. 키 릴리즈(Completed) 이벤트 유실로
	 * Hold 게이트 태그(State.Player.Sprinting)가 고착되는 것을 함께 정리한다. */
	virtual void FlushPressedKeys() override;

protected:
	/** 앱(창) 활성화 복귀 시 순수 게임플레이 상황이면 입력 모드/Ignore 잔류를 재정립.
	 * 창 전환으로 look 입력이 죽는(입력 모드 잔류) 문제의 방어. UI 컨텍스트 활성 시엔 개입하지 않음. */
	void HandleAppActivationChanged(bool bIsActive);
	FDelegateHandle AppActivationHandle;

	/** 해제 필수 시스템 메시지 모달 입력 잠금이 활성인지(Enter/Exit ModalMessageInput가 관리).
	 * 창 복귀 재정립(HandleAppActivationChanged)이 이 잠금을 깨지 않도록 판정에만 사용. */
	bool bModalMessageInputActive = false;

public:

protected:
	void HandleSessionStateChanged(ERetrieveSessionState Previous, ERetrieveSessionState NewState);
	void SwapActiveWidget(ERetrieveSessionState Previous, ERetrieveSessionState NewState);
	void UpdateInputMode(ERetrieveSessionState NewState);
	bool ShouldShowLoadingCover(ERetrieveSessionState Previous, ERetrieveSessionState NewState) const;

	TSubclassOf<UUserWidget> ResolveWidgetClass(ERetrieveSessionState State) const;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|UI")
	TSubclassOf<UUserWidget> MainMenuClass;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|UI")
	TSubclassOf<UUserWidget> HUDClass;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|UI")
	TSubclassOf<UUserWidget> ResultClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> ActiveTopLevelWidget;

	FGameplayMessageListenerHandle SessionListener;

#pragma endregion

#pragma region Menu Presentation (Camera & Loading)

protected:
	/** 세션 상태별 카메라 + 폰 가시성 처리 (메뉴 시네마틱 vs 게임플레이). */
	void HandleSessionPresentation(ERetrieveSessionState NewState);
	void ApplyMainMenuCamera(); // 로컬 폰 숨김 + 뷰 타겟 -> 시네 카메라
	void ApplyGameplayCamera(); // 로컬 폰 표시 + 뷰 타겟 블렌드 -> 폰
	AActor* FindMainMenuCamera() const;

	void ShowLoadingScreen();
	void HideLoadingScreen();

	UFUNCTION()
	void HandleLoadingScreenRemoved();

	/** 커버가 올라와 있는 동안 알림 표면들이 억제되도록 Channel.UI.RevealGate를 브로드캐스트합니다. */
	void BroadcastRevealGate(bool bBlocked);

	/** 부팅/메뉴 로딩 화면 위젯 (코스메틱). (빠른 이동용 WBP_LoadingCutscene이 아님) */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|UI")
	TSubclassOf<UUserWidget> LoadingScreenClass;
	
	/** 개발 편의: 새 게임/부팅 진입 시 로딩 커버 생략 (부활 커버는 항상 유지). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dev")
	bool bSkipEntryLoadingScreen = false;

	/** Persistant 레벨에 배치된 인게임 메인 메뉴 시네 카메라를 식별하는 액터 태그. */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Menu")
	FName MainMenuCameraTag = TEXT("MainMenuCamera");

	/** 새 게임 시 뷰 타겟 블렌드 (메뉴 시네 카메라 -> 게임플레이 폰). 로딩 화면이 가림. */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Menu")
	float MenuToGameplayBlendSeconds = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Menu")
	float LoadingScreenMinSeconds = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Menu")
	float LoadingScreenSafetySeconds = 10.0f;

	UPROPERTY()
	TObjectPtr<UUserWidget> ActiveLoadingScreen;

	bool bLoadingCoverActive = false;

	FTimerHandle LoadingScreenTimerHandle;
#pragma endregion

#pragma region HUD ViewModel

public:
	UFUNCTION(BlueprintPure, Category = "Retrieve|UI")
	UHUDViewModel* GetHUDViewModel() const { return HUDViewModelInstance; }

	/** 보스 전투 시작 시 호출합니다. BossHealth가 null이면 현재 보스 체력바 바인딩을 해제합니다. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI")
	void TryBindBossToHUD(URetrieveHealthComponent* BossHealth, FText BossName);

protected:
	/** HUDViewModelInstance를 생성하고 ActiveTopLevelWidget에 연결합니다. */
	void EnsureHUDViewModel();

	/** 둘 다 존재하면 PlayerStatus VM을 로컬 폰의 HealthComponent에 바인딩합니다. */
	void TryBindHealthToHUD();

	/** 로컬 폰의 ElementGaugeComponent를 ElementGaugeViewModel에 바인딩합니다. */
	void TryBindElementGaugeToHUD();

	/** HUD child boss bar instance에 BossStatus VM을 주입합니다. */
	void BindBossStatusViewModelToBossBarWidget();

	void ApplyBossStatusToBossBarWidget();

	/** VM을 해제합니다. HUD 위젯 제거 시 호출됩니다. */
	void ClearHUDViewModel();

	/**
	 * 대화/시네마틱 재생 중에는 인게임 HUD(ActiveTopLevelWidget)를 숨기고,
	 * 둘 다 끝나면 원래 가시성으로 복원한다. 대화 열기/닫기, 시네마틱 상태 변경,
	 * HUD 재생성 시점에 호출된다.
	 */
	void UpdateHUDNarrativeVisibility();

	/** 시네마틱 상태 변경 시 HUD 표시를 갱신하는 상시 리스너(BeginPlay 등록). */
	FGameplayMessageListenerHandle CinematicHUDVisibilityHandle;
	bool bHUDHiddenForNarrative = false;
	ESlateVisibility SavedHUDVisibilityForNarrative = ESlateVisibility::SelfHitTestInvisible;

	UPROPERTY()
	TObjectPtr<UHUDViewModel> HUDViewModelInstance;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|UI")
	FName HUDViewModelBindingName = TEXT("HUDViewModel");

	/**
	 * 아이템 획득 토스트 알림 위젯 클래스 (WBP_ToastManager).
	 * HUD와 독립적으로 InGame 상태에서만 Viewport에 추가/제거된다.
	 * WBP_HUD의 부모 클래스와 무관하게 동작한다.
	 *
	 * 설정: BP_RetrievePlayerController → Details → ToastManagerClass 슬롯
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|UI",
		meta = (DisplayName = "Toast Manager Class"))
	TSubclassOf<UUserWidget> ToastManagerClass;

	/** InGame 상태에서만 활성화되는 토스트 알림 위젯 인스턴스 */
	UPROPERTY()
	TObjectPtr<UUserWidget> ActiveToastManager;

	/** 시네마틱 중 뷰포트의 모든 최상위 위젯(HUD 루트, 퀵슬롯, 토스트 등 직접 AddToViewport된 것 전부)을
	 * 일괄 숨김/복원합니다. 예외: 로딩 커버, CinematicSubsystem의 오버레이(예외 UI).
	 * Channel.Cinematic.Changed 기반이라 재생 주체(서브시스템/치트)와 무관하게 동작합니다. */
	void SetHUDHiddenForCinematic(bool bHideForCinematic);

	/** 시네마틱 재생 중인지(GameState CinematicState 기준). InputKey는 뷰포트 직행 경로라 DisableInput의
	 * 영향을 받지 않으므로, 그 경로로 열리는 UI(ESC 시스템 메뉴/퀵슬롯 휠)는 이걸로 직접 차단합니다. */
	bool IsCinematicActive() const;

	/** 시네마틱 숨김을 적용한 위젯과 원래 가시성. 도중 위젯 파괴에 대비해 약참조. */
	TArray<TPair<TWeakObjectPtr<UUserWidget>, ESlateVisibility>> CinematicHiddenWidgets;
#pragma endregion

#pragma region Panels & Minimap

public:
	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI")
	void OpenExclusivePanel(TSubclassOf<URetrieveGamePanelWidget> PanelClass, FKey ToggleKey);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI")
	void CloseActivePanel();

	UFUNCTION(BlueprintPure, Category = "Retrieve|UI")
	URetrieveGamePanelWidget* GetActivePanel() const { return ActivePanel; }

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Map")
	void ToggleMinimapRotationMode();

	/** 메인 메뉴/시스템 메뉴/화톳불 UI에서 설정 버튼에 직접 연결할 진입점. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Settings")
	void OpenSettingsPanel();

	/** 메인메뉴 "불러오기" 버튼에서 직접 연결할 진입점. 슬롯 선택형 불러오기 창(WBP_LoadGame)을 연다. */
	UFUNCTION(BlueprintCallable, Exec, Category = "Retrieve|Menu")
	void OpenLoadGamePanel();

	/** 순수 게임플레이 중 ESC로 여는 시스템 메뉴. SystemMenuClass가 비어 있으면 무시된다. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI")
	void OpenSystemMenu();

	/** 조작키 안내 화면을 연다(시스템 메뉴의 "조작키 안내" 버튼에서 호출). 현재 패널은 교체된다. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI")
	void OpenControlsGuide();

	/** 상점 패널을 열고 InitializeShopPanel을 자동으로 호출합니다. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Shop")
	void OpenShopPanel(TSubclassOf<URetrieveGamePanelWidget> ShopPanelClass,
	                   URetrieveShopDefinitionAsset* ShopDefinition);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Shop")
	void OpenShopFromCurrentConversation(bool bOpenSellTab);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Shop")
	bool ReturnToShopConversation();

protected:
	bool TryHandleMinimapShortcut(FKey Key);
	bool TryHandlePanelShortcut(FKey Key);
	bool CanOpenPanel(const FRetrievePanelShortcutConfig& ShortcutConfig) const;
	void CenterActiveWorldMapPanel();
	void RemoveActivePanelImmediately();
	void HandleActivePanelCloseFallback();
	URetrieveMinimapWidget* FindMinimapWidgetInHUD() const;

	UFUNCTION()
	void HandleActivePanelCloseRequested();

	UFUNCTION()
	void HandleActivePanelCloseVFXFinished(FGameplayTag EffectTag);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|UI")
	TArray<FRetrievePanelShortcutConfig> PanelShortcuts;

	/** 기본 설정 화면. 별도 BP 배선 없이 SettingsPanelKey로도 열 수 있다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|Settings")
	TSoftClassPtr<URetrieveGamePanelWidget> SettingsPanelClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|Settings")
	FKey SettingsPanelKey = EKeys::F10;

	/** 메인메뉴 불러오기 화면(WBP_LoadGame). 기본값은 C++ 생성자에서 지정, BP에서 오버라이드 가능. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|Menu")
	TSoftClassPtr<URetrieveGamePanelWidget> LoadGamePanelClass;

	/** 시스템 메뉴 위젯(WBP_SystemMenu). BP_RetrievePlayerController에서 할당. 비어 있으면 ESC 시스템 메뉴는 비활성. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|UI")
	TSoftClassPtr<URetrieveGamePanelWidget> SystemMenuClass;

	/** 조작키 안내 위젯(WBP_ControlsGuide). 기본값은 C++ 생성자에서 지정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|UI")
	TSoftClassPtr<URetrieveGamePanelWidget> ControlsGuideClass;

	/** 상점 패널 위젯 클래스. BP_RetrievePlayerController에서 WBP_ShopPanel 할당 */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Shop")
	TSubclassOf<URetrieveGamePanelWidget> ShopPanelWidgetClass;

	// ── 인게임 퀵슬롯 라디얼 휠 ──────────────────────────────────────────
	// QuickSlotWheelKey를 누르고 있는 동안 휠을 열고, 떼면 마우스 방향 슬롯을 사용한다.
	void OpenQuickSlotWheel();
	void CloseQuickSlotWheelAndUse();

	/** 라디얼 휠을 여는(누르고 있는) 키. 기본 T. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|QuickSlot")
	FKey QuickSlotWheelKey = EKeys::T;

	/** 인게임 라디얼 휠 위젯 클래스 (WBP_QuickSlotWheel). */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|QuickSlot")
	TSubclassOf<URetrieveQuickSlotWheelWidget> QuickSlotWheelClass;

	/** 휠 슬롯 아이콘용 DataTable (WBP_InventoryPanel의 것과 동일). */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|QuickSlot")
	TObjectPtr<UDataTable> QuickSlotIconTable;

	UPROPERTY()
	TObjectPtr<URetrieveQuickSlotWheelWidget> QuickSlotWheelInstance;

	bool bQuickSlotWheelOpen = false;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|UI")
	TObjectPtr<URetrieveGamePanelWidget> ActivePanel;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|UI")
	TSubclassOf<URetrieveGamePanelWidget> ActivePanelClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|UI")
	int32 PanelZOrder = 50;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Map")
	bool bCenterWorldMapOnOpen = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Map")
	FKey MinimapRotationKey = EKeys::N;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Map")
	bool bEnableMinimapRotationShortcut = true;

	bool bActivePanelClosing = false;
	FTimerHandle ActivePanelCloseFallbackTimerHandle;

#pragma endregion

#pragma region Conversation / Dialogue

public:
	UFUNCTION(Server, Reliable)
	void Server_RequestDialogueAdvance(FGameplayTag TopicId);

	UFUNCTION(Client, Reliable)
	void Client_OpenConversation(AActor* NPC);

	void CloseConversation();

protected:
	void SetInputModeUIOnlyDuringConversation();
	void EnsureCinematicCloseListener();

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|UI")
	TSubclassOf<UUserWidget> ConversationWidgetClass;

	/** 대화 시작 시 플레이어가 상대를 바라보는 회전 보간 속도. 0 이하면 즉시 스냅. */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|UI")
	float ConversationFaceInterpSpeed = 10.f;

	UPROPERTY()
	TObjectPtr<UUserWidget> ConversationInstance;

	UPROPERTY()
	TObjectPtr<UConversationViewModel> ConversationVM;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|UI")
	FName ConversationViewModelBindingName = TEXT("ConversationViewModel");

	FGameplayMessageListenerHandle CinematicCloseHandle;

#pragma endregion

#pragma region Shop

protected:
	UPROPERTY()
	TObjectPtr<URetrieveShopDefinitionAsset> PendingShopDefinition;

	UPROPERTY()
	TObjectPtr<URetrieveShopComponent> PendingShopComponent;

	bool bPendingShopOpenSellTab = false;

	UPROPERTY()
	TObjectPtr<AActor> CurrentDialogueNPC;

	UPROPERTY()
	TObjectPtr<AActor> CurrentShopNPC;

	bool bCanReturnToShopConversation = false;

	FGameplayMessageListenerHandle ShopCommandHandle;

	void HandleShopOpenCommand(const FRetrieveLumenCommandPayload& Payload);

	// ── 상점/대화 NPC 포커스 카메라 ──────────────────────────────────────────
	// NPC는 풀 캐릭터(3인칭 카메라 리그 보유)라 그 카메라를 쓰면 NPC 뒤통수가 보인다.
	// 그래서 NPC 앞쪽에 임시 카메라 액터를 스폰해 NPC 정면을 프레이밍한다.

	/** 뷰 타깃 블렌드 시간(초). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Shop|Camera")
	float ShopCameraBlendTime = 0.6f;

	/** NPC 정면에서 떨어지는 거리(cm). 클수록 멀리서 본다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Shop|Camera")
	float ShopCameraDistance = 320.0f;

	/** 카메라 높이(cm, NPC 액터 기준). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Shop|Camera")
	float ShopCameraHeight = 135.0f;

	/** 카메라가 바라보는 지점의 높이(cm, NPC 액터 기준 — 보통 얼굴/가슴). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Shop|Camera")
	float ShopCameraLookAtHeight = 95.0f;

	/** 상점 NPC 포커스 카메라 시야각. 낮을수록 더 줌인된다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Shop|Camera", meta = (ClampMin = "20.0", ClampMax = "90.0"))
	float ShopCameraFOV = 55.0f;

	// Positive values move the NPC to the right side of the screen.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Shop|Camera")
	float ShopCameraFrameRightOffset = 70.0f;

	/** NPC 액터 Forward 기준 카메라 배치 각도(도, Z축 회전).
	 *  0 = 액터 Forward 쪽, 180 = 반대쪽. 메시 정면이 액터 Forward와 반대면 180이 정면. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Shop|Camera")
	float ShopCameraOrbitYaw = 180.0f;

	/** 상점 카메라 동안 플레이어 폰을 일시적으로 숨길지 여부. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Shop|Camera")
	bool bHidePlayerDuringShopCamera = true;

	// ── 일반 대화(다이얼로그) NPC 포커스 카메라 오버라이드 ────────────────────
	// Villager 등 상점이 아닌 NPC와의 대화에서 상점 카메라보다 더 확대하고,
	// 화면 좌측에 더 가깝게(FrameRightOffset을 낮춰) 배치하기 위한 값.

	/** 대화 포커스 카메라 거리(cm). 340까지 늘렸더니 시장 가판대 근처 NPC에서 카메라가 구조물과
	 *  걸려 화면이 깨졌다(충돌 검사가 없는 단순 스폰 배치라 거리를 늘릴수록 근처 지형/가판대에
	 *  걸릴 위험이 커짐). 280은 검증된 안전 거리라 되돌리고, 상반신 노출은 FOV로만 확보한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Dialogue|Camera")
	float DialogueCameraDistance = 280.0f;

	/** 대화 포커스 카메라 시야각. 카메라 위치(Distance/Height)는 검증된 값을 유지한 채 이 값만 넓혀서
	 *  상체 노출 범위를 늘린다(카메라를 움직이지 않으므로 주변 지형 충돌 위험이 없다). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Dialogue|Camera", meta = (ClampMin = "20.0", ClampMax = "90.0"))
	float DialogueCameraFOV = 64.0f;

	/** 대화 포커스 카메라의 좌우 프레이밍 오프셋. 음수면 NPC가 화면 좌측에 배치된다(대화창 요소는 우측/하단에 배치되므로). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Dialogue|Camera")
	float DialogueCameraFrameRightOffset = -70.0f;

	/** 대화 포커스 카메라 높이(cm, NPC 액터 기준). LookAtHeight보다 확실히 높게 두어 내려다보는 각도를
	 *  유지해야 시장 가판대 같은 낮은 장애물 너머로 NPC를 비출 수 있다(각도를 눕히면 장애물에 걸린다). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Dialogue|Camera")
	float DialogueCameraHeight = 145.0f;

	/** 대화 카메라가 바라보는 지점의 높이(cm, NPC 액터 기준). ShopCameraLookAtHeight(95)의 원 주석대로
	 *  이 프로젝트 캐릭터들은 95가 이미 "얼굴/가슴" 높이다(실사 비율보다 작은 스타일라이즈드 캐릭터).
	 *  145로 올려봤더니 머리 훨씬 위 허공을 보게 되어 오히려 캐릭터가 화면 아래로 밀려나는
	 *  역효과가 났다 — 검증된 95로 되돌리고, 상체 노출은 FOV로만 확보한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Dialogue|Camera")
	float DialogueCameraLookAtHeight = 95.0f;

	// ── 대화 투샷(OTS) 카메라 ─────────────────────────────────────────
	/** true면 대화 카메라를 플레이어+NPC가 함께 보이는 사선 투샷으로 배치한다. false면 기존 NPC 단독 샷. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Dialogue|Camera|TwoShot")
	bool bDialogueTwoShotCamera = true;

	/** 투샷: 플레이어 어깨 뒤로 물러나는 거리(cm, 플레이어→NPC 축 기준). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Dialogue|Camera|TwoShot")
	float DialogueTwoShotBackDistance = 240.0f;

	/** 투샷: 축에서 옆으로 비껴나는 거리(cm). +면 플레이어가 화면 좌측 전경, NPC가 우중앙에 잡힌다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Dialogue|Camera|TwoShot")
	float DialogueTwoShotSideOffset = 130.0f;

	/** 투샷 카메라 높이(cm, 플레이어 기준). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Dialogue|Camera|TwoShot")
	float DialogueTwoShotCameraHeight = 120.0f;

	/** 투샷 시선 지점의 플레이어→NPC 보간 비율(0=플레이어, 1=NPC). 클수록 NPC가 프레임 중심에 온다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Dialogue|Camera|TwoShot", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DialogueTwoShotLookAtBias = 0.65f;

	/** 투샷 시선 높이(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Dialogue|Camera|TwoShot")
	float DialogueTwoShotLookAtHeight = 85.0f;

	/** 투샷 시야각. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Dialogue|Camera|TwoShot", meta = (ClampMin = "20.0", ClampMax = "90.0"))
	float DialogueTwoShotFOV = 55.0f;

	/** 현재 상점 카메라(NPC 포커스)가 활성 상태인지 */
	bool bShopCameraActive = false;

	/** NPC 정면 프레이밍용으로 스폰해 재사용하는 카메라 액터. */
	UPROPERTY()
	TObjectPtr<ACameraActor> ShopFocusCameraActor;

	/** 카메라가 NPC를 포커스하는 동안 임시로 꺼둔(원래 켜져 있던) 상호작용 대상들. 복귀 시 다시 켠다. */
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> HiddenInteractionActorsForCamera;

	/** 위 목록이 이미 채워져 중복으로 다시 스캔/숨기지 않도록 하는 플래그. */
	bool bInteractionTargetsHiddenForCamera = false;

	/**
	 * TargetActor(상점 NPC 등)의 정면을 비추도록 카메라를 블렌드한다.
	 * 각 Override를 지정하지 않으면 대응하는 ShopCamera* 프로퍼티(상점 NPC 기준으로 보정된 값)를 사용한다.
	 * 캐릭터 종류/상황마다 메시 정면-액터 Forward 관계나 원하는 프레이밍이 달라(예: Villager 대화는 더 확대)
	 * 호출부에서 오버라이드할 수 있게 한다.
	 */
	void FocusCameraOnActor(AActor* TargetActor, TOptional<float> OrbitYawOverride = TOptional<float>(),
		TOptional<float> DistanceOverride = TOptional<float>(), TOptional<float> FOVOverride = TOptional<float>(),
		TOptional<float> FrameRightOffsetOverride = TOptional<float>(),
		TOptional<float> HeightOverride = TOptional<float>(), TOptional<float> LookAtHeightOverride = TOptional<float>());

	/**
	 * 대화용 투샷 카메라: 플레이어 어깨 뒤 사선에서 플레이어와 NPC가 함께 보이도록 배치한다.
	 * (플레이어 폰을 숨기지 않는다 — NPC 단독 샷과 달리 두 인물이 모두 프레임에 들어와야 한다.)
	 */
	void FocusDialogueTwoShotCamera(AActor* TargetActor);

	/** 플레이어 폰으로 뷰 타깃을 복귀한다. */
	void RestorePlayerCameraView();

	/** 현재 켜져 있는 모든 상호작용 대상(InteractionTarget)을 임시로 끄고 목록에 기록한다(NPC 포커스 카메라 동안 다른 NPC의 프롬프트가 뜨지 않도록). */
	void HideAllInteractionTargetsForCamera();

	/** HideAllInteractionTargetsForCamera로 꺼둔 상호작용 대상들을 원래대로 복귀한다. */
	void RestoreAllInteractionTargetsForCamera();

#pragma endregion

#pragma region Lumen Control

public:
	UFUNCTION(Server, Reliable)
	void Server_RequestRecallLumen();

	UFUNCTION(Server, Reliable)
	void Server_RequestLumenToggleWait();
#pragma endregion

#pragma region Interaction

public:
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Retrieve|Interaction")
	UActorComponent* GetInteractorComponent() const;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Interaction", meta = (MetaClass = "/Script/Engine.ActorComponent"))
	TSoftClassPtr<UActorComponent> InteractorComponentClass;
#pragma endregion

#pragma region Pawn Accessors & Misc

public:
	float GetSecondsSinceLastInput() const;

protected:
	UInventoryComponent* GetPawnInventoryComponent() const;
	UWeaponComponent* GetPawnWeaponComponent() const;

	double LastInputRealTimeSeconds = 0.0;

	// AttackFeedback 관련 멤버변수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Combat")
	TObjectPtr<UAttackFeedbackComponent> AttackFeedbackComponent;
#pragma endregion
};
