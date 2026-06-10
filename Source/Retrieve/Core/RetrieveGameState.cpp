#include "Core/RetrieveGameState.h"

#include "GameplayCueFunctionLibrary.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/PlayerState.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "Net/UnrealNetwork.h"
#include "Quest/QuestBranchComponent.h"

ARetrieveGameState::ARetrieveGameState(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bReplicates = true;
	QuestBranchComponent = CreateDefaultSubobject<UQuestBranchComponent>(TEXT("QuestBranch"));
}

void ARetrieveGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARetrieveGameState, SessionState);
	DOREPLIFETIME(ARetrieveGameState, HostPlayerState);
	DOREPLIFETIME(ARetrieveGameState, DialogueState);
	DOREPLIFETIME(ARetrieveGameState, CinematicState);
}

APawn* ARetrieveGameState::GetHostPawn() const
{
	return HostPlayerState ? HostPlayerState->GetPawn() : nullptr;
}

bool ARetrieveGameState::TransitionTo(ERetrieveSessionState NewState)
{
	if (!HasAuthority())
	{
		return false;
	}

	if (NewState == SessionState)
	{
		return false;
	}

	if (!IsLegalTransition(SessionState, NewState))
	{
		return false;
	}

	const ERetrieveSessionState Previous = SessionState;
	SessionState = NewState;
	BroadcastStateChange(Previous);

	return true;
}

void ARetrieveGameState::SetHostPlayerState(APlayerState* InPlayerState)
{
	if (!HasAuthority())
	{
		return;
	}

	if (HostPlayerState != nullptr || InPlayerState == nullptr)
	{
		return;
	}

	HostPlayerState = InPlayerState;
}

void ARetrieveGameState::OnRep_SessionState(ERetrieveSessionState Previous)
{
	BroadcastStateChange(Previous);
}

void ARetrieveGameState::BroadcastStateChange(ERetrieveSessionState Previous)
{
	FRetrieveSessionStatePayload Payload;
	Payload.PreviousState = Previous;
	Payload.NewState = SessionState;

	if (UWorld* World = GetWorld())
	{
		UGameplayMessageSubsystem::Get(World).BroadcastMessage(RetrieveGameplayTags::Channel_Session_StateChanged,
		                                                       Payload);
	}

	OnSessionStateChanged.Broadcast(Previous, SessionState);
}

bool ARetrieveGameState::IsLegalTransition(ERetrieveSessionState From, ERetrieveSessionState To)
{
	switch (From)
	{
	case ERetrieveSessionState::Loading:
		return To == ERetrieveSessionState::MainMenu;

	case ERetrieveSessionState::MainMenu:
		return To == ERetrieveSessionState::InGame;

	case ERetrieveSessionState::InGame:
		return To == ERetrieveSessionState::Result;

	case ERetrieveSessionState::Result:
		return To == ERetrieveSessionState::InGame || To == ERetrieveSessionState::MainMenu;

	default:
		return false;
	}
}

// ── Dialogue ────────────────────────────────────────────────────────────────

void ARetrieveGameState::SetActiveSpeaker(const FText& InSpeakerName)
{
	if (!HasAuthority())
	{
		return;
	}
	CurrentSpeakerName = InSpeakerName;
}

void ARetrieveGameState::RequestDialogue(const TArray<FText>& Lines, const TArray<FRetrieveDialogueTopic>& Topics,
                                         bool bShared, bool bHoldUntilReplaced)
{
	if (!HasAuthority())
	{
		return;
	}

	FRetrieveDialogueState Next;
	Next.SpeakerName = CurrentSpeakerName;
	Next.Lines = Lines;
	Next.Topics = Topics;
	Next.bSharedNarrative = bShared;
	Next.Serial = DialogueState.Serial + 1;
	Next.bHoldUntilReplaced = bHoldUntilReplaced;

	DialogueState = Next;
	OnRep_DialogueState();
}

void ARetrieveGameState::ClearDialogue()
{
	if (!HasAuthority())
	{
		return;
	}

	FRetrieveDialogueState Empty;
	Empty.Serial = DialogueState.Serial + 1;
	DialogueState = Empty;
	OnRep_DialogueState();
}

void ARetrieveGameState::OnRep_DialogueState()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FRetrieveDialoguePayload Message;
	Message.SpeakerName = DialogueState.SpeakerName;
	Message.Lines = DialogueState.Lines;
	Message.Topics = DialogueState.Topics;
	Message.bSharedNarrative = DialogueState.bSharedNarrative;
	Message.bHoldUntilReplaced = DialogueState.bHoldUntilReplaced;

	UGameplayMessageSubsystem::Get(World).BroadcastMessage(RetrieveGameplayTags::Channel_Dialogue_LineRequested,
	                                                       Message);
}

void ARetrieveGameState::AdvanceDialogue(FGameplayTag TopicId, APawn* Sovereign)
{
	if (!HasAuthority())
	{
		return;
	}

	const FDialogueRow* Row = FindDialogueRow(TopicId);
	if (!Row)
	{
		return;
	}

	switch (Row->Kind)
	{
	case ETopicKind::Story:
		RequestDialogue(Row->Lines, BuildFollowUpTopics(*Row), true);
		break;

	case ETopicKind::Command:
		if (Row->CommandChannel.IsValid())
		{
			FRetrieveLumenCommandPayload Message;
			Message.CommandTag = Row->CommandChannel;
			Message.Instigator = Sovereign;
			if (UWorld* World = GetWorld())
			{
				UGameplayMessageSubsystem::Get(World).BroadcastMessage(Row->CommandChannel, Message);
			}
		}
		RequestDialogue(Row->Lines, {}, true);
		break;

	case ETopicKind::Sigil:
		ApplySigilTopic(*Row, Sovereign);
		break;
	}
}

void ARetrieveGameState::ApplySigilTopic(const FDialogueRow& Row, APawn* Sovereign)
{
	if (!HasAuthority())
	{
		return;
	}

	UQuestBranchComponent* QuestComp = GetQuestBranchComponent();
	if (!QuestComp || QuestComp->IsStepCompleted(Row.SigilStepTag))
	{
		return; // 재진입 방지
	}
	if (Row.RequiresStep.IsValid() && !QuestComp->IsStepCompleted(Row.RequiresStep))
	{
		return; // 전제 조건 미충족
	}

	// 1. VFX 적용 — GameplayCue는 모든 클라이언트에 복제됨
	if (Row.VfxCue.IsValid() && Sovereign)
	{
		FGameplayCueParameters CueParams;
		CueParams.SourceObject = Sovereign;
		UGameplayCueFunctionLibrary::ExecuteGameplayCueOnActor(Sovereign, Row.VfxCue, CueParams);
	}

	// 2. 퀘스트 기록
	QuestComp->CompleteStep(Row.SigilStepTag);

	// 3. 이어서 대화
	RequestDialogue(Row.Lines, {}, true);
}

const FDialogueRow* ARetrieveGameState::FindDialogueRow(FGameplayTag TopicId) const
{
	if (!DialogueTable || !TopicId.IsValid())
	{
		return nullptr;
	}

	static const FString ContextString(TEXT("GameState_FindDialogueRow"));
	TArray<FDialogueRow*> Rows;
	DialogueTable->GetAllRows<FDialogueRow>(ContextString, Rows);
	for (const FDialogueRow* Row : Rows)
	{
		if (Row && Row->TopicId.MatchesTagExact(TopicId))
		{
			return Row;
		}
	}
	return nullptr;
}

TArray<FRetrieveDialogueTopic> ARetrieveGameState::BuildFollowUpTopics(const FDialogueRow& Row) const
{
	TArray<FRetrieveDialogueTopic> Result;
	if (!DialogueTable)
	{
		return Result;
	}

	const UQuestBranchComponent* QuestComp = GetQuestBranchComponent();
	static const FString ContextString(TEXT("GameState_BuildFollowUpTopics"));

	for (const FName& RowName : Row.FollowUpRows)
	{
		const FDialogueRow* Child = DialogueTable->FindRow<FDialogueRow>(RowName, ContextString, false);
		if (!Child)
		{
			continue;
		}
		if (Child->RequiresStep.IsValid() && (!QuestComp || !QuestComp->IsStepCompleted(Child->RequiresStep)))
		{
			continue;
		}
		if (Child->BlockedByStep.IsValid() && QuestComp && QuestComp->IsStepCompleted(Child->BlockedByStep))
		{
			continue;
		}
		Result.Add(FRetrieveDialogueTopic{ Child->TopicId, Child->Label, true, Child->Kind });
	}
	return Result;
	
}

// ── Cinematic (최소 구현) ────────────────────────────────────────────────────────────────

void ARetrieveGameState::SetCinematicActive(bool bInActive)
{
	if (!HasAuthority() || CinematicState.bActive == bInActive)
	{
		return;
	}
	CinematicState.bActive = bInActive;
	OnRep_CinematicState();
}

void ARetrieveGameState::OnRep_CinematicState()
{
	if (UWorld* World = GetWorld())
	{
		FRetrieveCinematicStatePayload Message;
		Message.bActive = CinematicState.bActive;
		UGameplayMessageSubsystem::Get(World).BroadcastMessage(RetrieveGameplayTags::Channel_Cinematic_Changed, Message);
	}
}
