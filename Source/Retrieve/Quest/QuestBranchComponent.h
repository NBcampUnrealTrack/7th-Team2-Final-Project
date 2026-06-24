#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "QuestBranchComponent.generated.h"

struct FQuestStep;

/**
 * 완료된 퀘스트의 유일한 목록. ARetrieveGameState에 위치합니다.
 * 각 DT_QuestStep 행의 Prerequisites를 통해 수호자 처치 순서에 상관 없이 진행됩니다.
 */
UCLASS(ClassGroup = "Retrieve", meta = (BlueprintSpawnableComponent))
class RETRIEVE_API UQuestBranchComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQuestBranchComponent();

	/** 호스트 전용. 선행 조건이 충족되고 아직 완료되지 않은 경우에만 StepTag를 추가하고,
	 *  스텝이 새로 완료되면 true를 반환합니다. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Quest")
	bool CompleteStep(FGameplayTag StepTag);

	UFUNCTION(BlueprintPure, Category = "Retrieve|Quest")
	bool IsStepCompleted(FGameplayTag StepTag) const;
	
	/** 해당 스텝 행의 UnlockElementTag Getter. (<Element>Empowered 행만 태그를 포함함.)
	 *  ApplySigilTopic이 GameplayEvent.Core.Absorb를 발동하기 위해 읽습니다. */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Quest")
	FGameplayTag GetUnlockElementForStep(FGameplayTag StepTag) const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Quest")
	FGameplayTag GetCurrentTrackerStep() const { return CurrentTrackerStep; }

	/** 호스트 전용. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Quest")
	void RecordChoice(FName ChoiceId, FGameplayTag Pick);

	UFUNCTION(BlueprintPure, Category = "Retrieve|Quest")
	FGameplayTag GetChoice(FName ChoiceId) const;

	/** 호스트 전용. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Quest")
	void ResetForTest();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Quest")
	TObjectPtr<UDataTable> QuestStepTable;

	UPROPERTY(ReplicatedUsing = OnRep_CompletedSteps)
	TArray<FGameplayTag> CompletedSteps;

	TArray<FGameplayTag> LastSeenCompletedSteps;

	UPROPERTY(Replicated)
	FGameplayTag CurrentTrackerStep;

	/** 호스트 로컬 전용 */
	TMap<FName, FGameplayTag> ChoiceHistory;

	UFUNCTION()
	void OnRep_CompletedSteps();

	const FQuestStep* FindRow(FGameplayTag StepTag) const;
	void EvaluateAutoCompletions();

	void BroadcastStepChangedLocal(FGameplayTag NewStep);
	void BroadcastGuardianDefeatedLocal(FGameplayTag GuardianElement);
	void BroadcastSealUnlockedLocal();
};
