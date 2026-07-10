#include "UI/ViewModels/ConversationViewModel.h"

#include "Components/World/RetrieveDialogueComponent.h"
#include "Components/World/RetrieveShopComponent.h"
#include "Core/RetrieveGameState.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Player/RetrievePlayerController.h"
#include "Quest/QuestBranchComponent.h"

#define LOCTEXT_NAMESPACE "Retrieve.Conversation"

void UConversationViewModel::Initialize(UWorld* World, APlayerController* InOwningPlayerController)
{
	WorldPtr = World;
	OwningPlayerController = InOwningPlayerController;
	if (!World)
	{
		return;
	}

	if (!ListenerHandle.IsValid())
	{
		ListenerHandle = UGameplayMessageSubsystem::Get(World).RegisterListener<FRetrieveDialoguePayload>(
			RetrieveGameplayTags::Channel_Dialogue_LineRequested,
			[WeakThis = TWeakObjectPtr<UConversationViewModel>(this)]
		(FGameplayTag Channel, const FRetrieveDialoguePayload& Message)
			{
				if (UConversationViewModel* VM = WeakThis.Get())
				{
					VM->HandleLineRequested(Channel, Message);
				}
			});
	}

	if (const ARetrieveGameState* GS = World->GetGameState<ARetrieveGameState>())
	{
		const FRetrieveDialogueState& State = GS->GetDialogueState();
		if (State.Lines.Num() > 0 || State.Topics.Num() > 0)
		{
			FRetrieveDialoguePayload Seed;
			Seed.SpeakerName = State.SpeakerName;
			Seed.Lines = State.Lines;
			Seed.Topics = State.Topics;
			Seed.bSharedNarrative = State.bSharedNarrative;
			Seed.bHoldUntilReplaced = State.bHoldUntilReplaced;
			Seed.CompletesStep = State.CompletesStep;
			Seed.bAutoEndAfterLines = State.bAutoEndAfterLines;
			HandleLineRequested(RetrieveGameplayTags::Channel_Dialogue_LineRequested, Seed);
		}
	}
}

void UConversationViewModel::Deinitialize()
{
	if (ListenerHandle.IsValid())
	{
		if (UWorld* World = WorldPtr.Get())
		{
			UGameplayMessageSubsystem::Get(World).UnregisterListener(ListenerHandle);
		}
		ListenerHandle = FGameplayMessageListenerHandle();
	}
}

void UConversationViewModel::BeginDestroy()
{
	Deinitialize();
	Super::BeginDestroy();
}

void UConversationViewModel::BuildOpeningTopicsFor(AActor* NPC)
{
	Topics.Reset();
	Lines.Reset();
	SpeakerName = FText::GetEmpty();
	LineIndex = 0;
	CurrentCompletesStep = FGameplayTag();
	bCurrentAutoEndAfterLines = false;

	UWorld* World = OwningPlayerController.IsValid() ? OwningPlayerController->GetWorld() : WorldPtr.Get();
	ARetrieveGameState* GS = World ? World->GetGameState<ARetrieveGameState>() : nullptr;
	UQuestBranchComponent* Quest = GS ? GS->GetQuestBranchComponent() : nullptr;
	const UDataTable* Table = GS ? GS->GetDialogueTable() : nullptr;

	FGameplayTag SpeakerTag;
	if (const URetrieveDialogueComponent* DialogueComponent = NPC ? NPC->FindComponentByClass<URetrieveDialogueComponent>() : nullptr)
	{
		SpeakerTag = DialogueComponent->SpeakerTag;
		SpeakerName = DialogueComponent->SpeakerDisplayName;
		Lines = DialogueComponent->DefaultGreetingLines;
	}

	if (Table && Quest && SpeakerTag.IsValid())
	{
		static const FString ContextString(TEXT("ConversationVM_BuildOpeningTopics"));
		TArray<FDialogueRow*> AllRows;
		Table->GetAllRows<FDialogueRow>(ContextString, AllRows);
	
		TArray<FDialogueRow*> Eligible;
		for (FDialogueRow* Row : AllRows)
		{
			if (!Row || Row->SpeakerTag != SpeakerTag)
			{
				continue;
			}
			if (Row->RequiresStep.IsValid() && !Quest->IsStepCompleted(Row->RequiresStep))
			{
				continue; // 조건 미충족
			}
			if (Row->BlockedByStep.IsValid() && Quest->IsStepCompleted(Row->BlockedByStep))
			{
				continue; // 이미 완료된 토픽 (BlockedByStep)
			}
			Eligible.Add(Row);
		}

		Eligible.Sort([](const FDialogueRow& A, const FDialogueRow& B)
		{
			return A.Order != B.Order ? A.Order < B.Order : A.Priority < B.Priority;
		});
		
		// bReplacesGreeting 행 중 가장 높은 Priority가 기본 그리팅 대신 오프닝 제공
		const FDialogueRow* OpeningRow = nullptr;
		for (const FDialogueRow* Row : Eligible)
		{
			if (Row->bReplacesGreeting && (!OpeningRow || Row->Priority > OpeningRow->Priority))
			{
				OpeningRow = Row;
			}
		}

		bool bExclusiveOpening = false;
		if (OpeningRow)
		{
			Lines = OpeningRow->Lines; // 인사말 대체
			CurrentCompletesStep = OpeningRow->CompletesStep;
			bCurrentAutoEndAfterLines = OpeningRow->bAutoEndAfterLines;
			Topics = GS->BuildFollowUpTopics(*OpeningRow); // 이 행의 후속 토픽
			bExclusiveOpening = OpeningRow->bAutoEndAfterLines && OpeningRow->FollowUpRows.Num() == 0;
		}

		if (!bExclusiveOpening)
		{
			for (const FDialogueRow* Row : Eligible)
			{
				if (Row->bReplacesGreeting)
				{
					continue;
				}
				Topics.Add(FRetrieveDialogueTopic{Row->TopicId, Row->Label, true, Row->Kind});
			}
		}
	}

	if (NPC && NPC->FindComponentByClass<URetrieveShopComponent>())
	{
		Topics.RemoveAll([](const FRetrieveDialogueTopic& Topic)
		{
			return Topic.TopicId.MatchesTagExact(RetrieveGameplayTags::Topic_ShopNPC_OpenShop)
				|| Topic.TopicId.MatchesTagExact(RetrieveGameplayTags::Topic_ShopNPC_Buy)
				|| Topic.TopicId.MatchesTagExact(RetrieveGameplayTags::Topic_ShopNPC_Sell);
		});

		const auto FindShopTopicFromTable = [Table, SpeakerTag](FGameplayTag TopicId) -> const FDialogueRow*
		{
			if (!Table || !SpeakerTag.IsValid() || !TopicId.IsValid())
			{
				return nullptr;
			}

			static const FString ContextString(TEXT("ConversationVM_FindShopTopic"));
			TArray<FDialogueRow*> AllRows;
			Table->GetAllRows<FDialogueRow>(ContextString, AllRows);

			for (const FDialogueRow* Row : AllRows)
			{
				if (!Row)
				{
					continue;
				}

				if (Row->SpeakerTag == SpeakerTag && Row->TopicId.MatchesTagExact(TopicId))
				{
					return Row;
				}
			}

			return nullptr;
		};

		if (const FDialogueRow* BuyRow = FindShopTopicFromTable(RetrieveGameplayTags::Topic_ShopNPC_Buy))
		{
			Topics.Insert(FRetrieveDialogueTopic{
				BuyRow->TopicId,
				BuyRow->Label,
				true,
				BuyRow->Kind
			}, 0);
		}

		if (const FDialogueRow* SellRow = FindShopTopicFromTable(RetrieveGameplayTags::Topic_ShopNPC_Sell))
		{
			Topics.Insert(FRetrieveDialogueTopic{
				SellRow->TopicId,
				SellRow->Label,
				true,
				SellRow->Kind
			}, 1);
		}
	}

	AppendGoodbyeIfMissing();
	bTopicsEnabled = OwningPlayerController.IsValid() && OwningPlayerController->HasAuthority();
	BroadcastBeatFields();
	
	if (GetShowChoices())
	{
		HandleBeatEnded();
	}
}

void UConversationViewModel::Advance()
{
	if (GetShowChoices())
	{
		if (bCurrentAutoEndAfterLines && !HasSelectableTopics())
		{
			if (ARetrievePlayerController* RetrievePC = Cast<ARetrievePlayerController>(OwningPlayerController.Get()))
			{
				RetrievePC->CloseConversation();
			}
		}
		return; // 이미 마지막 라인 (토픽 표시 중)
	}
	
	++LineIndex;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetCurrentLine);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetShowChoices);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetShowTopicsPanel);
	
	if (GetShowChoices())
	{
		HandleBeatEnded(); // 마지막 라인 도달 = 비트 종료
	}
}

void UConversationViewModel::OnTopicSelected(FGameplayTag TopicId)
{
	APlayerController* PC = OwningPlayerController.Get();
	if (!PC)
	{
		return;
	}

	if (!TopicId.IsValid())
	{
		// Goodbye/ESC: 로컬에서 닫기
		if (ARetrievePlayerController* RetrievePC = Cast<ARetrievePlayerController>(PC))
		{
			RetrievePC->CloseConversation();
		}
		return;
	}

	if (TopicId.MatchesTagExact(RetrieveGameplayTags::Topic_ShopNPC_Buy)
		|| TopicId.MatchesTagExact(RetrieveGameplayTags::Topic_ShopNPC_Sell))
	{
		if (ARetrievePlayerController* RetrievePC = Cast<ARetrievePlayerController>(PC))
		{
			RetrievePC->OpenShopFromCurrentConversation(
				TopicId.MatchesTagExact(RetrieveGameplayTags::Topic_ShopNPC_Sell));
		}
		return;
	}

	if (PC->HasAuthority())
	{
		if (ARetrieveGameState* GS = PC->GetWorld() ? PC->GetWorld()->GetGameState<ARetrieveGameState>() : nullptr)
		{
			GS->AdvanceDialogue(TopicId, PC->GetPawn());
		}
	}
	else if (ARetrievePlayerController* RetrievePC = Cast<ARetrievePlayerController>(PC))
	{
		RetrievePC->Server_RequestDialogueAdvance(TopicId);
	}
}

void UConversationViewModel::SelectTopicByIndex(int32 Index)
{
	// 선택 불가(비호스트 공유 대화)이거나 범위 밖/비활성 토픽은 무시.
	if (!bTopicsEnabled || !Topics.IsValidIndex(Index) || !Topics[Index].bEnabled)
	{
		return;
	}

	// 마지막 Goodbye(무효 TopicId)도 OnTopicSelected가 대화 종료로 자연히 처리.
	OnTopicSelected(Topics[Index].TopicId);
}

void UConversationViewModel::HandleLineRequested(FGameplayTag Channel, const FRetrieveDialoguePayload& Message)
{
	SpeakerName = Message.SpeakerName;
	Lines = Message.Lines;
	Topics = Message.Topics;
	CurrentCompletesStep = Message.CompletesStep;
	bCurrentAutoEndAfterLines = Message.bAutoEndAfterLines;
	LineIndex = 0;
	AppendGoodbyeIfMissing();

	bTopicsEnabled = !Message.bSharedNarrative || (OwningPlayerController.IsValid() && OwningPlayerController->
		HasAuthority());

	BroadcastBeatFields();
	
	if (GetShowChoices())
	{
		HandleBeatEnded();
	}
}

void UConversationViewModel::HandleBeatEnded()
{
	if (CurrentCompletesStep.IsValid() && OwningPlayerController.IsValid() && OwningPlayerController->HasAuthority())
	{
		UWorld* World = OwningPlayerController->GetWorld();
		if (ARetrieveGameState* GS = World ? World->GetGameState<ARetrieveGameState>() : nullptr)
		{
			if (UQuestBranchComponent* Quest = GS->GetQuestBranchComponent())
			{
				Quest->CompleteStep(CurrentCompletesStep);
			}
		}
	}
}

void UConversationViewModel::AppendGoodbyeIfMissing()
{
	const bool bHasGoodbye = Topics.ContainsByPredicate(
		[](const FRetrieveDialogueTopic& Topic) { return !Topic.TopicId.IsValid(); });

	if (bCurrentAutoEndAfterLines && !HasSelectableTopics())
	{
		return;
	}
	
	if (!bHasGoodbye)
	{
		Topics.Add(FRetrieveDialogueTopic{
			FGameplayTag(),
			LOCTEXT("Goodbye", "대화 종료"), true, ETopicKind::Story
		});
	}
}

void UConversationViewModel::BroadcastBeatFields()
{
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetSpeakerName);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetCurrentLine);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetShowChoices);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetTopics);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetTopicsEnabled);
}

bool UConversationViewModel::HasSelectableTopics() const
{
	return Topics.ContainsByPredicate([](const FRetrieveDialogueTopic& Topic) { return Topic.TopicId.IsValid(); });
}

#undef LOCTEXT_NAMESPACE
