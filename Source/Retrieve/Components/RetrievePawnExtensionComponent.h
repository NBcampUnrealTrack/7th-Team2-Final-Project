#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveAbilitySet.h"
#include "Components/GameFrameworkInitStateInterface.h"
#include "Components/PawnComponent.h"
#include "RetrievePawnExtensionComponent.generated.h"

class URetrievePawnData;
class URetrieveAbilitySystemComponent;

UCLASS()
class RETRIEVE_API URetrievePawnExtensionComponent : public UPawnComponent, public IGameFrameworkInitStateInterface
{
	GENERATED_BODY()

public:
	URetrievePawnExtensionComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	static const FName NAME_ActorFeatureName;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//~ Begin IGameFrameworkInitStateInterface interface
	virtual FName GetFeatureName() const override { return NAME_ActorFeatureName; }
	virtual bool CanChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState,
	                                FGameplayTag DesiredState) const override;
	virtual void HandleChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState,
	                                   FGameplayTag DesiredState) override;
	virtual void OnActorInitStateChanged(const FActorInitStateChangedParams& Params) override;
	virtual void CheckDefaultInitialization() override;
	//~ End IGameFrameworkInitStateInterface interface

	/** Returns the pawn extension component if one exists on the specified actor. */
	static URetrievePawnExtensionComponent* FindPawnExtensionComponent(const AActor* Actor)
	{
		return Actor ? Actor->FindComponentByClass<URetrievePawnExtensionComponent>() : nullptr;
	}

	const URetrievePawnData* GetPawnData() const { return PawnData; }
	void SetPawnData(const URetrievePawnData* InPawnData);

	UFUNCTION(BlueprintPure, Category = "Retrieve|Pawn")
	URetrieveAbilitySystemComponent* GetRetrieveAbilitySystemComponent() const { return AbilitySystemComponent; }

	/** owning actor의 ASC 캐시 (소버린은 PlayerState 소유 ASC, AI는 Pawn 소유 ASC) */
	void InitializeAbilitySystem(URetrieveAbilitySystemComponent* InASC, AActor* InOwnerActor);

	/** Should be called by the owning pawn to remove itself as the avatar of the ability system. */
	void UninitializeAbilitySystem();

	/** `PawnData->CharacterStatsRow`를 새로 부여된 `UCombatAttributeSet`에 적용 */
	void ApplyCharacterStatsRow();

	void HandleControllerChanged();
	void HandlePlayerStateReplicated();
	void SetupPlayerInputComponent();
	
protected:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(EditInstanceOnly, ReplicatedUsing = OnRep_PawnData, Category = "Retrieve|Pawn")
	TObjectPtr<const URetrievePawnData> PawnData;

	UFUNCTION()
	void OnRep_PawnData();

	UPROPERTY()
	TObjectPtr<URetrieveAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY() // Transient
	FRetrieveAbilitySet_GrantedHandles GrantedHandles;
};
