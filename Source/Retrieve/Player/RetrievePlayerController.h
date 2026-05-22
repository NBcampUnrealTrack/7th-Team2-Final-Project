#pragma once

#include "CoreMinimal.h"
#include "Core/RetrieveSessionState.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "RetrievePlayerController.generated.h"

class ARetrievePlayerState;

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
	
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PlayerTick(float DeltaTime) override;
	
	void HandleSessionStateChanged(ERetrieveSessionState NewState);
	void SwapActiveWidget(ERetrieveSessionState NewState);
	void UpdateInputMode(ERetrieveSessionState NewState);
	
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
};
