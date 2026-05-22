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
	
	void HandleSessionStateChanged(ERetrieveSessionState NewState);
	void SwapActiveWidget(ERetrieveSessionState NewState);
	void UpdateInputMode(ERetrieveSessionState NewState);
	
	TSubclassOf<UUserWidget> ResolveWidgetClass(ERetrieveSessionState State) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|UI")
	TArray<FRetrievePanelShortcutConfig> PanelShortcuts;
	
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|UI")
	TSubclassOf<UUserWidget> MainMenuClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|UI")
	TSubclassOf<UUserWidget> HUDClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|UI")
	TSubclassOf<UUserWidget> ResultClass;

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
	
	FGameplayMessageListenerHandle SessionListener;

	UFUNCTION()
	void HandleActivePanelCloseRequested();

	bool TryHandleMinimapShortcut(FKey Key);
	bool TryHandlePanelShortcut(FKey Key);
	bool TryHandleConsumableSlotShortcut(FKey Key);
	bool CanOpenPanel(const FRetrievePanelShortcutConfig& ShortcutConfig) const;
	void CenterActiveWorldMapPanel();
	URetrieveMinimapWidget* FindMinimapWidgetInHUD() const;
	UInventoryComponent* GetPawnInventoryComponent() const;
	UWeaponComponent* GetPawnWeaponComponent() const;
};
