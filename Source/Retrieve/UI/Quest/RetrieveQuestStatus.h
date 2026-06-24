#pragma once

#include "CoreMinimal.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Quest/QuestBranchComponent.h"

namespace QuestStatus
{
	// 퀘스트가 잠금 해제됐는가: UnlockRequirements가 전부 CompletedSteps에 있는가 (빈 목록 = 처음부터 해제)
	inline bool IsQuestUnlocked(const FQuestDefinition& Quest, const UQuestBranchComponent& Branch)
	{
		for (const FGameplayTag& Req : Quest.UnlockRequirements)
		{
			if (!Branch.IsStepCompleted(Req))
			{
				return false;
			}
		}
		return true;
	}

	// 퀘스트가 완료됐는가: 모든 목표의 CompletionTag가 CompletedSteps에 있는가.
	// 목표가 없으면 "완료"로 치지 않는다(영원히 Active로 두기보다 후보에서 제외 — 아래 Recompute 참고)
	inline bool AreAllObjectivesComplete(const FQuestDefinition& Quest, const UQuestBranchComponent& Branch)
	{
		if (Quest.Objectives.Num() == 0)
		{
			return false;
		}
		for (const FQuestObjective& Objective : Quest.Objectives)
		{
			if (!Branch.IsStepCompleted(Objective.CompletionTag))
			{
				return false;
			}
		}
		return true;
	}
}
