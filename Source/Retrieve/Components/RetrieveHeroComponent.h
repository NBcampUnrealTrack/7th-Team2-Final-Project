#pragma once

#include "CoreMinimal.h"
#include "Components/GameFrameworkInitStateInterface.h"
#include "Components/PawnComponent.h"
#include "RetrieveHeroComponent.generated.h"

struct FInputActionValue;

/**
 * 플레이어 전용. PawnExtensionComponent가 DataInitialized 상태에 도달하는 것을 기다린 후, Enhanced Input을 바인딩합니다.
 * AI 폰은 이 컴포넌트를 갖지 않습니다.
 */
UCLASS(Blueprintable, Meta=(BlueprintSpawnableComponent))
class RETRIEVE_API URetrieveHeroComponent : public UPawnComponent, public IGameFrameworkInitStateInterface
{
	GENERATED_BODY()
	
public:
	URetrieveHeroComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	static const FName NAME_ActorFeatureName;
	static const FName NAME_BindInputsNow;
	
	static URetrieveHeroComponent* FindHeroComponent(const AActor* Actor)
	{
		return Actor? Actor->FindComponentByClass<URetrieveHeroComponent>() : nullptr;
	}
	
	void InitializePlayerInput(UInputComponent* PlayerInputComponent);
	
	//~ Begin IGameFrameworkInitStateInterface interface
	virtual FName GetFeatureName() const override { return NAME_ActorFeatureName; }
	virtual bool CanChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) const override;
	virtual void HandleChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) override;
	virtual void OnActorInitStateChanged(const FActorInitStateChangedParams& Params) override;
	virtual void CheckDefaultInitialization() override;
	//~ End IGameFrameworkInitStateInterface interface
	
protected:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void BindPlayerInputs();

	void Input_Move(const FInputActionValue& InputActionValue);
	void Input_Look(const FInputActionValue& InputActionValue);

	void Input_AbilityInputTagPressed(FGameplayTag InputTag);
	void Input_AbilityInputTagReleased(FGameplayTag InputTag);

private:
	/** SetupPlayerInputComponent 시점에 캐시됨; 초기화가 DataInitialized에 도달하면 바인딩을 재시도에 사용됩니다 */
	UPROPERTY(Transient)
	TObjectPtr<UInputComponent> PendingInputComponent;

	bool bInputsBound = false;
};
