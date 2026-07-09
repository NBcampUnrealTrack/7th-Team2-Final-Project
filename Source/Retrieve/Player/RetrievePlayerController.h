#pragma once

#include "CoreMinimal.h"
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

	/** 언스턱/강제 리스폰. 정규 사망 흐름(로딩 커버 포함)으로 마지막 체크포인트에 리스폰. 포즈 팝업에서 호출. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Menu")
	void RequestUnstuck();

	/** 게임 종료(애플리케이션 종료). 포즈 팝업에서 호출. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Menu")
	void RequestQuitGame();

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

	/** 메인 메뉴/일시정지/화톳불 UI에서 설정 버튼에 직접 연결할 진입점. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Settings")
	void OpenSettingsPanel();

	/** 메인메뉴 "불러오기" 버튼에서 직접 연결할 진입점. 슬롯 선택형 불러오기 창(WBP_LoadGame)을 연다. */
	UFUNCTION(BlueprintCallable, Exec, Category = "Retrieve|Menu")
	void OpenLoadGamePanel();

	/** 조작키 안내 화면을 연다(일시정지 메뉴의 "조작키 안내" 버튼에서 호출). 현재 패널은 교체된다. */
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

	/** 메인메뉴 불러오기 화면(WBP_LoadGame). BP_RetrievePlayerController에서 할당. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|Menu")
	TSoftClassPtr<URetrieveGamePanelWidget> LoadGamePanelClass;

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

	/** 대화 포커스 카메라 거리(cm). 상점보다 가까이 당겨 확대감을 준다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Dialogue|Camera")
	float DialogueCameraDistance = 220.0f;

	/** 대화 포커스 카메라 시야각. 상점보다 낮춰 더 확대되게 한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Dialogue|Camera", meta = (ClampMin = "20.0", ClampMax = "90.0"))
	float DialogueCameraFOV = 42.0f;

	/** 대화 포커스 카메라의 좌우 프레이밍 오프셋. 상점보다 낮춰(또는 음수로) NPC를 화면 좌측에 가깝게 배치한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Dialogue|Camera")
	float DialogueCameraFrameRightOffset = 20.0f;

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
		TOptional<float> FrameRightOffsetOverride = TOptional<float>());

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
