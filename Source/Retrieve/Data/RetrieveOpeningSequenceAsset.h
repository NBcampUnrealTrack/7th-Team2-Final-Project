#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "RetrieveOpeningSequenceAsset.generated.h"

/** 비트가 발생할 때 수행하는 동작. */
UENUM(BlueprintType)
enum class EOpeningBeatKind : uint8
{
	/** 이름으로 DT_SystemMessage 행을 요청 */
	SystemMessageById,
	/** 원시 시스템 메시지 텍스트를 인라인으로 브로드캐스트(테이블 행 없이 빠른 일회성 처리) */
	SystemMessageText,
	/** 호스트 전용 CompleteStep - 이후 퀘스트 토스트는 UQuestNotificationSubsystem이 도출 */
	CompleteQuestStep
};

/** 타이밍이 있는 오프닝 비트 하나. DelayBeforeSeconds는 이전 비트 발생 시점부터 측정됩니다. */
USTRUCT(BlueprintType)
struct FOpeningBeat
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Opening")
	EOpeningBeatKind Kind = EOpeningBeatKind::SystemMessageText;

	/** 이 비트를 발생시키기 전, 이전 비트로부터 대기할 시간(초). */
	UPROPERTY(EditAnywhere, Category = "Opening", meta = (ClampMin = "0.0"))
	float DelayBeforeSeconds = 1.5f;

	UPROPERTY(EditAnywhere, Category = "Opening",
		meta = (EditCondition = "Kind == EOpeningBeatKind::SystemMessageById", EditConditionHides))
	FName SystemMessageRowName = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Opening",
		meta = (EditCondition = "Kind == EOpeningBeatKind::SystemMessageText", EditConditionHides, MultiLine = true))
	FText MessageText;

	UPROPERTY(EditAnywhere, Category = "Opening",
		meta = (EditCondition = "Kind == EOpeningBeatKind::SystemMessageText", EditConditionHides, ClampMin = "0.5"))
	float MessageDuration = 5.0f;

	UPROPERTY(EditAnywhere, Category = "Opening",
		meta = (EditCondition = "Kind == EOpeningBeatKind::CompleteQuestStep", EditConditionHides, Categories =
			"Quest.Step"))
	FGameplayTag QuestStepTag;
};

/** 순서가 있는 오프닝 타임라인. BP_RetrieveGameMode -> OpeningSequence에 할당 */
UCLASS()
class RETRIEVE_API URetrieveOpeningSequenceAsset : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, Category = "Opening")
	TArray<FOpeningBeat> Beats;
};
