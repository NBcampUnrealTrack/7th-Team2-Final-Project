#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Core/RetrieveSessionState.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "RetrieveGameState.generated.h"

class UQuestBranchComponent;
class UGuardianCoreSpawnerComponent;
struct FDialogueRow;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRetrieveSessionStateChanged, ERetrieveSessionState, Previous,
                                             ERetrieveSessionState, New);

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
	
	UFUNCTION(BlueprintPure, Category = "Retrieve|Quest")
	UQuestBranchComponent* GetQuestBranchComponent() const { return QuestBranchComponent; }
	
	// ---- Dialogue / Cinematic ----
	const FRetrieveDialogueState& GetDialogueState() const { return DialogueState; }
	const FRetrieveCinematicState& GetCinematicState() const { return CinematicState; }
	const UDataTable* GetDialogueTable() const { return DialogueTable; }
	const UDataTable* GetQuestTable() const { return QuestTable; }

	void RequestDialogue(const TArray<FText>& Lines, const TArray<FRetrieveDialogueTopic>& Topics, bool bShared = true,
	                     bool bHoldUntilReplaced = false);
	void AdvanceDialogue(FGameplayTag TopicId, APawn* Sovereign);
	void ApplySigilTopic(const FDialogueRow& Row, APawn* Sovereign);
	void ClearDialogue();
	void SetActiveSpeaker(const FText& InSpeakerName);
	void SetCinematicActive(bool bInActive);
	
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
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest")
	TObjectPtr<UQuestBranchComponent> QuestBranchComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|GuardianCore")
	TObjectPtr<UGuardianCoreSpawnerComponent> GuardianCoreSpawner;
	
	// ---- Dialogue ----
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Dialogue")
	TObjectPtr<UDataTable> DialogueTable;
	
	// --- Quest content ---
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Quest")
	TObjectPtr<UDataTable> QuestTable;

	UPROPERTY(ReplicatedUsing = OnRep_DialogueState)
	FRetrieveDialogueState DialogueState;

	UPROPERTY(ReplicatedUsing = OnRep_CinematicState)
	FRetrieveCinematicState CinematicState;
	
	FText CurrentSpeakerName;
	
	UFUNCTION()
	void OnRep_SessionState(ERetrieveSessionState Previous);
	
	UFUNCTION()
	void OnRep_DialogueState();

	UFUNCTION()
	void OnRep_CinematicState();
	
	const FDialogueRow* FindDialogueRow(FGameplayTag TopicId) const;
	TArray<FRetrieveDialogueTopic> BuildFollowUpTopics(const FDialogueRow& Row) const;
	
	static bool IsLegalTransition(ERetrieveSessionState From, ERetrieveSessionState To);
	
	void BroadcastStateChange(ERetrieveSessionState Previous);
};
