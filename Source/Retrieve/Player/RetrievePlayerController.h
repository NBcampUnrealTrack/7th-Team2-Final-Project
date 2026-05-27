#pragma once

#include "CoreMinimal.h"
#include "Core/RetrieveSessionState.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "InputCoreTypes.h"
#include "RetrievePlayerController.generated.h"

class ARetrievePlayerState;
class UInventoryComponent;
class URetrieveGamePanelWidget;
class URetrieveMinimapWidget;
class UUserWidget;
class UWeaponComponent;
class UHUDViewModel;

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
	
public:
	ARetrievePlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	ARetrievePlayerState* GetRetrievePlayerState() const;
	
	UFUNCTION(BlueprintPure, Category = "Retrieve|UI")
	UHUDViewModel* GetHUDViewModel() const { return HUDViewModelInstance; }
	
	UFUNCTION(Server, Reliable)
	void Server_RequestNewGame();
	
	UFUNCTION(Server, Reliable)
	void Server_RequestRetry();
	
	UFUNCTION(Server, Reliable)
	void Server_RequestQuitToMenu();

	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI")
	void OpenExclusivePanel(TSubclassOf<URetrieveGamePanelWidget> PanelClass, FKey ToggleKey);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI")
	void CloseActivePanel();

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Map")
	void ToggleMinimapRotationMode();
	
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;
	virtual void AcknowledgePossession(APawn* InPawn) override;
	
	void HandleSessionStateChanged(ERetrieveSessionState NewState);
	void SwapActiveWidget(ERetrieveSessionState NewState);
	void UpdateInputMode(ERetrieveSessionState NewState);
	
	/** HUDViewModelInstance를 생성하고 ActiveTopLevelWidget에 연결합니다. */
	void EnsureHUDViewModel();
	
	/** 둘 다 존재하면 PlayerStatus VM을 로컬 폰의 HealthComponent에 바인딩합니다. */
	void TryBindHealthToHUD();
	
	/** VM을 해제합니다. HUD 위젯 제거 시 호출됩니다. */
	void ClearHUDViewModel();
	
	TSubclassOf<UUserWidget> ResolveWidgetClass(ERetrieveSessionState State) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|UI")
	TArray<FRetrievePanelShortcutConfig> PanelShortcuts;
	
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|UI")
	TSubclassOf<UUserWidget> MainMenuClass;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|UI")
	TSubclassOf<UUserWidget> HUDClass;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|UI")
	TSubclassOf<UUserWidget> ResultClass;

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
	
	UPROPERTY()
	TObjectPtr<UUserWidget> ActiveTopLevelWidget;

	/** InGame 상태에서만 활성화되는 토스트 알림 위젯 인스턴스 */
	UPROPERTY()
	TObjectPtr<UUserWidget> ActiveToastManager;
	
	UPROPERTY()
	TObjectPtr<UHUDViewModel> HUDViewModelInstance;
	
	/** WBP_HUD의 MVVM 패널에서 설정된 ViewModel 바인딩 이름 */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|UI")
	FName HUDViewModelBindingName = TEXT("HUDViewModel");
	
	FGameplayMessageListenerHandle SessionListener;

	UFUNCTION()
	void HandleActivePanelCloseRequested();

	UFUNCTION()
	void HandleActivePanelCloseVFXFinished(FGameplayTag EffectTag);

	bool TryHandleMinimapShortcut(FKey Key);
	bool TryHandlePanelShortcut(FKey Key);
	bool TryHandleConsumableSlotShortcut(FKey Key);
	bool CanOpenPanel(const FRetrievePanelShortcutConfig& ShortcutConfig) const;
	void CenterActiveWorldMapPanel();
	void RemoveActivePanelImmediately();
	URetrieveMinimapWidget* FindMinimapWidgetInHUD() const;
	UInventoryComponent* GetPawnInventoryComponent() const;
	UWeaponComponent* GetPawnWeaponComponent() const;

	bool bActivePanelClosing = false;
};
