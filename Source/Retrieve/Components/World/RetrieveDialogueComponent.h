#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "RetrieveDialogueComponent.generated.h"

class UAnimSequenceBase;
class UAnimMontage;
class USkeletalMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDialogueClosedSignature, AActor*, Instigator);

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

	/**
	 * 설정하면 Animation 대신 이 몽타주를 AnimBP의 Slot(예: ABP_ShopNPC의 DefaultSlot)으로 재생한다.
	 * AnimBP 상태머신(Idle/Run 블렌드)이 계속 아래에서 돌아가며 몽타주가 위에 얹히므로,
	 * 순찰 중인 Villager도 이동 블렌드가 끊기지 않고 자연스럽게 반응 동작을 재생할 수 있다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dialogue|Animation")
	TObjectPtr<UAnimMontage> Montage;

	/** true이면 재생 후 대화 애니메이션으로 자동 복귀 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dialogue|Animation")
	bool bAutoReturnToTalking = true;

	/** 자동 복귀까지 대기 시간 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dialogue|Animation", meta = (ClampMin = "0.1"))
	float ReturnDelay = 2.0f;
};

/** 대화 종료 시 지급할 수 있는 아이템 후보. 가중치 기반으로 하나를 뽑는다. */
USTRUCT(BlueprintType)
struct FRetrieveDialogueItemReward
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dialogue|Rewards")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dialogue|Rewards", meta = (Categories = "Item"))
	FGameplayTag ItemCategoryTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dialogue|Rewards", meta = (ClampMin = "1"))
	int32 Quantity = 1;

	/** 가중치. 값이 클수록 더 자주 선택된다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dialogue|Rewards", meta = (ClampMin = "0.0"))
	float Weight = 1.f;
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


	/** 서버에서 대화 UI가 정상적으로 닫힌 뒤 호출됩니다. */
	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Dialogue")
	FOnDialogueClosedSignature OnDialogueClosed;

	void NotifyDialogueClosed(AActor* Instigator) { OnDialogueClosed.Broadcast(Instigator); }

	// URetrieveInteractionResponseComponent.OnApplied에 바인딩됨 (호스트 전용)
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Dialogue")
	void HandleInteract(AActor* Instigator);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dialogue")
	FText SpeakerDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dialogue")
	FGameplayTag SpeakerTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dialogue")
	TArray<FText> DefaultGreetingLines;

	/**
	 * 이 NPC의 상호작용 프롬프트 문구(기본 "대화하기"). 시선 게이트가 상호작용 존을 재활성화할 때,
	 * 매니저(InteractionTarget)의 InteractionText가 비어 있거나 플러그인 기본값 "Interact"이면
	 * 이 문구로 교체한다. 레벨 배치 인스턴스에 옛 "Interact"가 직렬화돼 BP 기본값을 가리는 경우를
	 * 런타임에 교정하기 위한 것이며, "Interact"가 아닌 정당한 커스텀 문구(상점 등)는 덮어쓰지 않는다.
	 * 비우면 교정하지 않는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dialogue")
	FText InteractionPromptText;

	const FText& GetInteractionPromptText() const { return InteractionPromptText; }

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

	/** 설정하면 대화 시작 시 TalkingAnimation 대신 이 몽타주를 AnimBP Slot으로 재생한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dialogue|Animation")
	TObjectPtr<UAnimMontage> GreetingMontage;

	// ── 아이템 보상 (Details 패널에서 설정) ──────────────────────────────────

	/** 대화 종료 시 아이템 지급을 시도할 확률(0~1). 0이면 지급 없음(기본값, 상점 NPC 등 기존 동작 불변). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dialogue|Rewards", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ItemRewardChance = 0.f;

	/** ItemRewardChance 굴림에 성공했을 때 가중치 기반으로 하나를 골라 지급할 후보 목록. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dialogue|Rewards")
	TArray<FRetrieveDialogueItemReward> ItemRewardPool;

	/** 랜덤 보상 최대 지급 횟수(이 NPC 기준, 세션 동안 유지). 대화 횟수와 무관하게 이 횟수까지만 지급. 0 이하 = 무제한. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dialogue|Rewards", meta = (ClampMin = "0"))
	int32 MaxItemRewardGrants = 3;

	/** 대화 종료 시 PlayerController(서버 권한)에서 호출. 확률 굴림 후 성공하면 Instigator 인벤토리에 아이템을 지급한다. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Dialogue|Rewards")
	bool TryGrantItemReward(AActor* Instigator);

	// ── 가위바위보 내기 (Details 패널에서 설정) ──────────────────────────────

	/** true면 대화 선택지에 "내기하기(가위바위보)"가 추가된다. 3연승 시 RpsRewardPool에서 보상 지급. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dialogue|Bet")
	bool bEnableRpsBet = false;

	/** 3연승 보상 후보(가중치 선택). 좋은 보상을 넣는다. 비어 있으면 축하 대사만 나온다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dialogue|Bet")
	TArray<FRetrieveDialogueItemReward> RpsRewardPool;

	/** 3연승 보상 최대 지급 횟수(이 NPC 기준, 세션 동안 유지). 소진되면 내기 토픽이 숨겨진다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dialogue|Bet", meta = (ClampMin = "0"))
	int32 MaxRpsRewardGrants = 2;

	/** 내기 토픽을 노출할 수 있는지(활성 + 보상 잔여). ConversationViewModel이 대화 선택지 구성 시 조회. */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Dialogue|Bet")
	bool CanOfferRpsBet() const;

	/**
	 * 내기 관련 토픽(Dialogue.Bet.*)이면 서버 권한으로 처리하고 true를 반환한다.
	 * PlayerController::Server_RequestDialogueAdvance에서 DT_Dialogue 조회 전에 호출된다.
	 */
	bool HandleRpsBetTopic(FGameplayTag TopicId, APawn* PlayerPawn);

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

	/** 가중치 기반으로 Pool에서 하나를 골라 Instigator 인벤토리에 지급한다. */
	bool GrantWeightedReward(const TArray<FRetrieveDialogueItemReward>& Pool, AActor* Instigator) const;

	/** 저장 키로 쓸 SpeakerTag 결정(컴포넌트 SpeakerTag → 루멘 캐릭터 SpeakerTag 순). */
	FGameplayTag ResolveSpeakerTagForSave() const;

	/** 저장 파일(WorldState) 기준 지급 횟수 조회. 키/서브시스템이 없으면 세션 카운터 폴백. */
	int32 GetRewardGrantCount(bool bRpsBet) const;

	/** 지급 횟수 +1 — 저장 파일에 영속화(+세션 폴백 카운터도 함께 증가). */
	void IncrementRewardGrantCount(bool bRpsBet);

	/** 랜덤 보상 지급 횟수 세션 폴백(SpeakerTag 미설정 NPC용). */
	int32 ItemRewardGrantCount = 0;

	/** 3연승 보상 지급 횟수 세션 폴백. */
	int32 RpsRewardGrantCount = 0;

	/** 현재 내기 세션의 연승 수. 대화 시작 시 리셋. */
	int32 RpsWinStreak = 0;
};
