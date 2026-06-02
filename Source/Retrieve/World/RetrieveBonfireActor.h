#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RetrieveBonfireActor.generated.h"

class URetrieveMapIconComponent;
class URetrieveInteractionResponseComponent;
class UArrowComponent;
class UStaticMeshComponent;
class URetrieveSaveSubsystem;
class UUserWidget;

/** Broadcast once when a bonfire is activated for the first time. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnBonfireActivatedSignature, FText, ActivatedDisplayName, FName, ActivatedBonfireId);

/** Bonfire interaction point for activation, saving, and fast travel. */
UCLASS(Blueprintable)
class RETRIEVE_API ARetrieveBonfireActor : public AActor
{
	GENERATED_BODY()

public:
	ARetrieveBonfireActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Stable identifier used by saving and fast travel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Bonfire")
	FName BonfireId;

	/** Name shown in world map and fast travel UI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Bonfire")
	FText DisplayName;

	/** Whether this bonfire has already been activated. */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Bonfire")
	bool IsActivated() const { return bIsActivated; }

	/** Activates this bonfire and returns true only for the first activation. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Bonfire")
	bool ActivateBonfire();

	/** Used by the HUD to announce the first activation. */
	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Bonfire")
	FOnBonfireActivatedSignature OnBonfireActivated;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Bonfire|UI")
	TSoftClassPtr<UUserWidget> BonfireMenuClass;

	/** External InteractionManager enum value for reactivating a completed interaction. */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bonfire|Interaction")
	uint8 PersistentFinishMethodValue = 3;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	/** Arrival transform used as the fast travel destination. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<UArrowComponent> ArrivalPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<URetrieveMapIconComponent> MapIconComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<URetrieveInteractionResponseComponent> InteractionComponent;

	/** External InteractionManager target stored as UActorComponent to avoid a plugin type dependency. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<UActorComponent> InteractionTargetComponent;

private:
	UFUNCTION()
	void HandleInteractionApplied(AActor* InteractionInstigator);

	bool TryOpenBonfireMenu(AActor* InteractionInstigator);
	void RestoreGameInputAfterMenuClosed();

	/** Keep the external InteractionManager target reusable after interaction. */
	void ConfigurePersistentInteractionTarget();

	/** Restore activation state from the save subsystem during BeginPlay. */
	void TryRestoreActivationFromSave();

	UPROPERTY(VisibleInstanceOnly, Category = "Retrieve|Bonfire")
	bool bIsActivated = false;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> ActiveBonfireMenu;

	UPROPERTY(Transient)
	TObjectPtr<APlayerController> BonfireMenuPlayerController;
};
