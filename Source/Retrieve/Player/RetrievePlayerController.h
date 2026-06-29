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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|UI")
	TSubclassOf<URetrieveGamePanelWidget> PanelClass;

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
	//BeginPlay, EndPlay, PlayerTick, InputKey, AcknowledgePossession
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

	/**
	 * 메뉴 - 새 게임 진입점. 로컬에 코스메틱 로딩 화면을 표시한 후,
	 * 호스트 전용 Server_RequestNewGame (-> HandleNewGame -> MainMenu->InGame)을 호출합니다. W_MainMenu에서 호출.
	 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Menu")
	void RequestNewGame();

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

	/** 부팅/메뉴 로딩 화면 위젯 (코스메틱). (빠른 이동용 WBP_LoadingCutscene이 아님) */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|UI")
	TSubclassOf<UUserWidget> LoadingScreenClass;
	
	/** 개발 편의: 새 게임/부팅 진입 시 로딩 커버 생략 (부활 커버는 항상 유지). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dev")
	bool bSkipEntryLoadingScreen = true;

	/** Persistant 레벨에 배치된 인게임 메인 메뉴 시네 카메라를 식별하는 액터 태그. */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Menu")
	FName MainMenuCameraTag = TEXT("MainMenuCamera");

	/** 새 게임 시 뷰 타겟 블렌드 (메뉴 시네 카메라 -> 게임플레이 폰). 로딩 화면이 가림. */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Menu")
	float MenuToGameplayBlendSeconds = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Menu")
	float LoadingScreenMinSeconds = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Menu")
	float LoadingScreenSafetySeconds = 10.0f;

	UPROPERTY()
	TObjectPtr<UUserWidget> ActiveLoadingScreen;

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

	/** 현재 상점 카메라(NPC 포커스)가 활성 상태인지 */
	bool bShopCameraActive = false;

	/** NPC 정면 프레이밍용으로 스폰해 재사용하는 카메라 액터. */
	UPROPERTY()
	TObjectPtr<ACameraActor> ShopFocusCameraActor;

	/** TargetActor(상점 NPC 등)의 정면을 비추도록 카메라를 블렌드한다. */
	void FocusCameraOnActor(AActor* TargetActor);

	/** 플레이어 폰으로 뷰 타깃을 복귀한다. */
	void RestorePlayerCameraView();

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
