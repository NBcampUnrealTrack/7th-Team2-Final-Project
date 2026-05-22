#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Core/RetrieveSessionState.h"
#include "RetrieveGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRetrieveSessionStateChanged, ERetrieveSessionState, Previous, ERetrieveSessionState, New);

UCLASS()
class RETRIEVE_API ARetrieveGameState : public AGameStateBase
{
	GENERATED_BODY()
	
public:
	ARetrieveGameState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	UFUNCTION(BlueprintPure, Category = "Retrieve|Session")
	ERetrieveSessionState GetSessionState() const { return SessionState;}
	
	UFUNCTION(BlueprintPure, Category = "Retrieve|Session")
	APlayerState* GetHostPlayerState() const { return HostPlayerState; }
	
	UFUNCTION(BlueprintPure, Category = "Retrieve|Session")
	APawn* GetHostPawn() const;
	
	/** 서버 전용. 전환이 수락되면 true를 반환합니다. */
	bool TransitionTo(ERetrieveSessionState NewState);

	/** 서버 전용. 첫 번째 PostLogin에서 한 번만 설정하고, 이후 호출은 무시됩니다. */
	void SetHostPlayerState(APlayerState* InPlayerState);
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	/** SessionState 변경 시 ReplicatedUsing → OnRep → 로컬 브로드캐스트 */
	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Session")
	FOnRetrieveSessionStateChanged OnSessionStateChanged;
	
protected:
	UPROPERTY(ReplicatedUsing = OnRep_SessionState)
	ERetrieveSessionState SessionState = ERetrieveSessionState::Loading;
	
	UPROPERTY(Replicated)
	TObjectPtr<APlayerState> HostPlayerState;
	
	UFUNCTION()
	void OnRep_SessionState(ERetrieveSessionState Previous);
	
	static bool IsLegalTransition(ERetrieveSessionState From, ERetrieveSessionState To);
	
	void BroadcastStateChange(ERetrieveSessionState Previous);
};
