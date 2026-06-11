#include "Quest/QuestBranchComponent.h"

#include "Core/RetrieveGameState.h"
#include "Core/RetrieveSessionState.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Engine/DataTable.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "Net/UnrealNetwork.h"

UQuestBranchComponent::UQuestBranchComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UQuestBranchComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UQuestBranchComponent, CompletedSteps);
	DOREPLIFETIME(UQuestBranchComponent, CurrentTrackerStep);
}

bool UQuestBranchComponent::IsStepCompleted(FGameplayTag StepTag) const
{
	return CompletedSteps.Contains(StepTag);
}

FGameplayTag UQuestBranchComponent::GetUnlockElementForStep(FGameplayTag StepTag) const
{
	const FQuestStep* Row = FindRow(StepTag);
	return Row ? Row->UnlockElementTag : FGameplayTag();
}

const FQuestStep* UQuestBranchComponent::FindRow(FGameplayTag StepTag) const
{
	if (!QuestStepTable)
	{
		return nullptr;
	}

	static const FString ContextString(TEXT("QuestBranch_FindRow"));
	TArray<FQuestStep*> Rows;
	
	QuestStepTable->GetAllRows<FQuestStep>(ContextString, Rows);
	for (const FQuestStep* Row : Rows)
	{
		if (Row && Row->StepTag.MatchesTagExact(StepTag))
		{
			return Row;
		}
	}
	return nullptr;
}

bool UQuestBranchComponent::CompleteStep(FGameplayTag StepTag)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}
	if (IsStepCompleted(StepTag))
	{
		return false;
	}

	const FQuestStep* Row = FindRow(StepTag);
	if (!Row)
	{
		return false;
	}

	// 선행 조건 확인
	for (const FGameplayTag& Prereq : Row->Prerequisites)
	{
		if (!IsStepCompleted(Prereq))
		{
			return false;
		}
	}

	CompletedSteps.Add(StepTag);
	CurrentTrackerStep = StepTag;

	BroadcastStepChangedLocal(StepTag);

	if (Row->UnlockElementTag.IsValid())
	{
		BroadcastGuardianDefeatedLocal(Row->UnlockElementTag);
	}
	if (StepTag.MatchesTag(RetrieveGameplayTags::Quest_Step_SealUnlocked))
	{
		BroadcastSealUnlockedLocal();
	}

	if (Row->bAdvancesSessionToResult)
	{
		if (ARetrieveGameState* GS = Cast<ARetrieveGameState>(GetOwner()))
		{
			GS->TransitionTo(ERetrieveSessionState::Result);
		}
	}

	EvaluateAutoCompletions();
	return true;
}

void UQuestBranchComponent::EvaluateAutoCompletions()
{
	if (!QuestStepTable)
	{
		return;
	}

	static const FString ContextString(TEXT("QuestBranch_EvalAuto"));
	TArray<FQuestStep*> Rows;
	
	QuestStepTable->GetAllRows<FQuestStep>(ContextString, Rows);
	for (const FQuestStep* Row : Rows)
	{
		if (!Row || !Row->bAutoCompleteWhenPrereqsMet || IsStepCompleted(Row->StepTag))
		{
			continue;
		}

		bool bAllMet = true;
		for (const FGameplayTag& Prereq : Row->Prerequisites)
		{
			if (!IsStepCompleted(Prereq))
			{
				bAllMet = false;
				break;
			}
		}
		if (bAllMet)
		{
			CompleteStep(Row->StepTag); // 태그가 채워지면 종료
		}
	}
}

void UQuestBranchComponent::OnRep_CompletedSteps()
{
	for (const FGameplayTag& Tag : CompletedSteps)
	{
		if (LastSeenCompletedSteps.Contains(Tag))
		{
			continue;
		}

		BroadcastStepChangedLocal(Tag);

		if (const FQuestStep* Row = FindRow(Tag))
		{
			if (Row->UnlockElementTag.IsValid())
			{
				BroadcastGuardianDefeatedLocal(Row->UnlockElementTag);
			}
		}
		if (Tag.MatchesTag(RetrieveGameplayTags::Quest_Step_SealUnlocked))
		{
			BroadcastSealUnlockedLocal();
		}
	}
	LastSeenCompletedSteps = CompletedSteps;
}

void UQuestBranchComponent::BroadcastStepChangedLocal(FGameplayTag NewStep)
{
	if (UWorld* World = GetWorld())
	{
		FRetrieveQuestStepPayload Message;
		Message.StepTag = NewStep;
		UGameplayMessageSubsystem::Get(World).BroadcastMessage(
			RetrieveGameplayTags::Channel_Quest_StepChanged, Message);
	}
}

void UQuestBranchComponent::BroadcastGuardianDefeatedLocal(FGameplayTag GuardianElement)
{
	if (UWorld* World = GetWorld())
	{
		FRetrieveGuardianDefeatedPayload Message;
		Message.GuardianElement = GuardianElement;
		UGameplayMessageSubsystem::Get(World).BroadcastMessage(
			RetrieveGameplayTags::Channel_Quest_GuardianDefeated, Message);
	}
}

void UQuestBranchComponent::BroadcastSealUnlockedLocal()
{
	if (UWorld* World = GetWorld())
	{
		FRetrieveQuestStepPayload Message;
		Message.StepTag = RetrieveGameplayTags::Quest_Step_SealUnlocked;
		UGameplayMessageSubsystem::Get(World).BroadcastMessage(
			RetrieveGameplayTags::Channel_Quest_SealUnlocked, Message);
	}
}

void UQuestBranchComponent::RecordChoice(FName ChoiceId, FGameplayTag Pick)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	ChoiceHistory.Add(ChoiceId, Pick);
}

FGameplayTag UQuestBranchComponent::GetChoice(FName ChoiceId) const
{
	if (const FGameplayTag* Found = ChoiceHistory.Find(ChoiceId))
	{
		return *Found;
	}
	return FGameplayTag();
}

void UQuestBranchComponent::ResetForTest()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	CompletedSteps.Reset();
	LastSeenCompletedSteps.Reset();
	CurrentTrackerStep = FGameplayTag();
	ChoiceHistory.Empty();
}
