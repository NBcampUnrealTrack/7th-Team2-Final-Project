#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "RetrieveDialogueComponent.generated.h"

class UAnimSequenceBase;
class USkeletalMeshComponent;

/** 특정 대화 선택지에 연동된 NPC 애니메이션 항목 */
USTRUCT(BlueprintType)
struct FNPCDialogueAnimEntry
{
	GENERATED_BODY()

	/** 이 애니메이션을 트리거하는 대화 선택지 TopicId */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dialogue|Animation")
	FGameplayTag TopicId;

	/** 재생할 애니메이션 (AnimSequence 또는 AnimMontage 모두 가능) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dialogue|Animation")
	TObjectPtr<UAnimSequenceBase> Animation;

	/** true이면 재생 후 대화 애니메이션으로 자동 복귀 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dialogue|Animation")
	bool bAutoReturnToTalking = true;

	/** 자동 복귀까지 대기 시간 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dialogue|Animation", meta = (ClampMin = "0.1"))
	float ReturnDelay = 2.0f;
};

/**
 * 대화형 NPC라면 누구나(루멘 포함) 부착할 수 있는 재사용 가능한 대화 트리거.
 * URetrieveInteractionResponseComponent.OnApplied(호스트 전용)에 바인딩되어,
 * 상호작용한 Sovereign의 컨트롤러에 대화 뷰를 엽니다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RETRIEVE_API URetrieveDialogueComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URetrieveDialogueComponent();

	// URetrieveInteractionResponseComponent.OnApplied에 바인딩됨 (호스트 전용)
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Dialogue")
	void HandleInteract(AActor* Instigator);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dialogue")
	FText SpeakerDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dialogue")
	FGameplayTag SpeakerTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dialogue")
	TArray<FText> DefaultGreetingLines;

	/** 이 NPC와의 대화가 끝날 때(Goodbye/ESC 포함) 이 퀘스트 스텝을 완료합니다. TODO: 튜토리얼 전용, 추후 삭제 예정. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dialogue", meta = (Categories = "Quest.Step"))
	FGameplayTag CompleteStepOnConversationEnd;

	// ── 애니메이션 (Details 패널에서 설정) ──────────────────────────────────

	/** 대화 외 평상시 루프 재생할 유휴 애니메이션 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dialogue|Animation")
	TObjectPtr<UAnimSequenceBase> IdleAnimation;

	/** 플레이어가 대화를 걸었을 때 루프 재생할 대화 중 애니메이션 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dialogue|Animation")
	TObjectPtr<UAnimSequenceBase> TalkingAnimation;

	/** 특정 대화 선택지 선택 시 재생할 애니메이션 목록 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dialogue|Animation")
	TArray<FNPCDialogueAnimEntry> TopicAnimations;

	/** 대화 시작 시 PlayerController에서 호출 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Dialogue|Animation")
	void PlayGreetingAnimation();

	/** 선택지 선택 시 PlayerController에서 호출 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Dialogue|Animation")
	void PlayTopicAnimation(FGameplayTag TopicId);

	/** 대화 종료 시 PlayerController에서 호출. IdleAnimation으로 복귀합니다. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Dialogue|Animation")
	void ReturnToIdle();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void OpenConversationFor(AActor* Instigator);

	/**
	 * InteractionTarget 컴포넌트의 FinishMethod를 ReactivateAfterCompleted(3)으로
	 * 런타임 강제 설정합니다. BonfireActor::ConfigurePersistentInteractionTarget과 동일한 방식.
	 */
	void ConfigureInteractionTarget();

	UPROPERTY(EditAnywhere, Category = "Retrieve|Dialogue")
	bool bAutoBindResponseComponent = true;

private:
	bool bBoundToResponseComponent = false;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> CachedMesh;

	FTimerHandle AnimReturnTimerHandle;
};
