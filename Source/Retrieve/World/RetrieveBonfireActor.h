#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "RetrieveBonfireActor.generated.h"

class URetrieveMapIconComponent;
class URetrieveInteractionResponseComponent;
class UArrowComponent;
class UStaticMeshComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class URetrieveSaveSubsystem;
class UBonfireMenuWidget;

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

	virtual void PostLoad() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

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
	TSoftClassPtr<UBonfireMenuWidget> BonfireMenuClass;

	/** External InteractionManager enum value for reactivating a completed interaction. */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bonfire|Interaction")
	uint8 PersistentFinishMethodValue = 3;

	/** Delay before opening the bonfire menu when no interaction montage is configured. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Bonfire|Interaction")
	float BonfireMenuFallbackOpenDelay = 0.0f;

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

	/** 활성화 시 재생되는 불꽃 VFX. 비활성화 상태에서는 꺼져 있다가 ActivateBonfire() 호출 시 켜진다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<UNiagaraComponent> FireVFXComponent;

	/** FireVFXComponent에 사용할 Niagara System. BP에서 교체 가능. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|Bonfire|VFX",
		meta = (DisplayName = "불꽃 VFX System"))
	TObjectPtr<UNiagaraSystem> FireVFXSystem;

private:
	void EnsureBonfireId();

	UFUNCTION()
	void HandleInteractionApplied(AActor* InteractionInstigator);

	void OpenPendingBonfireMenu();
	float GetBonfireMenuOpenDelay() const;

	bool TryOpenBonfireMenu(AActor* InteractionInstigator) const;

	/** Keep the external InteractionManager target reusable after interaction. */
	void ConfigurePersistentInteractionTarget() const;

	/** Restore activation state from the save subsystem during BeginPlay. */
	void TryRestoreActivationFromSave();

	void ApplyActivatedState(bool bRegisterWithSave);
	void ApplyBonfireVisualState();
	void HandleDeferredVisualStateSync();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Bonfire",
		meta = (DisplayName = "Is Activated", AllowPrivateAccess = "true"))
	bool bIsActivated = false;

	FTimerHandle DeferredVisualStateSyncTimerHandle;
	FTimerHandle BonfireMenuOpenTimerHandle;
	TWeakObjectPtr<AActor> PendingMenuInstigator;
};
