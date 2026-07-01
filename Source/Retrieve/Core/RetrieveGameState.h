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

	/** 호스트 권한. 플레이어가 가장 최근에 휴식하거나 활성화한 모닥불을 기록. */
	void SetLastCheckpointBonfire(FName BonfireId);

	UFUNCTION(BlueprintPure, Category = "Retrieve|Checkpoint")
	FName GetLastCheckpointBonfireId() const { return LastCheckpointBonfireId; }

	/**
	 * 리스폰 트랜스폼을 결정: 마지막으로 휴식한 모닥불의 ArrivalPoint, 없으면 기본 시작 모닥불, 없으면 첫 번째 PlayerStart.
	 * 항상 스폰 가능한 트랜스폼을 반환합니다.
	 */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Checkpoint")
	FTransform GetLastCheckpointOrFallback() const;

	/** 호스트 권한. 미설정 시 LastCheckpointBonfireId를 기본 시작 모닥불로 시딩. */
	void SeedDefaultCheckpointIfUnset();
	
	UFUNCTION(BlueprintPure, Category = "Retrieve|Quest")
	UQuestBranchComponent* GetQuestBranchComponent() const { return QuestBranchComponent; }
	
	// ---- Dialogue / Cinematic / Bark ----
	const FRetrieveDialogueState& GetDialogueState() const { return DialogueState; }
	const FRetrieveCinematicState& GetCinematicState() const { return CinematicState; }
	const UDataTable* GetDialogueTable() const { return DialogueTable; }
	const UDataTable* GetQuestTable() const { return QuestTable; }
	const UDataTable* GetBarkTable() const { return BarkTable; }

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
	
	/** 호스트 권한. 첫 휴식 전까지는 NAME_None (이후 GetLastCheckpointOrFallback이 폴백) */
	UPROPERTY(Replicated)
	FName LastCheckpointBonfireId;
	
	/** 첫 휴식 전 새 게임에서 스폰/리스폰되는 모닥불. BP_RetrieveGameState에서 설정. */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Checkpoint")
	FName DefaultStartBonfireId;
	
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

	// --- Bark content ---
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bark")
	TObjectPtr<UDataTable> BarkTable;

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
