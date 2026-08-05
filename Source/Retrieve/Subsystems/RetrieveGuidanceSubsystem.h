#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RetrieveGuidanceSubsystem.generated.h"

/**
 * "지금 뭘 해야 하는지"를 게임 전체에서 한 곳으로 모으는 길잡이 서브시스템.
 *
 * 유저테스트에서 가장 큰 피드백이 "뭘 해야 할지 모르겠다 / 무슨 게임인지 모르겠다"였다.
 * 원인은 목표 정보가 퀘스트 트래커에만, 그것도 월드가 살아 있는 동안에만 존재한다는 것.
 * 그래서 현재 목표를 GameInstance 수명으로 캐시해 두고
 *   - 로딩 화면 브리핑
 *   - 목표 재확인(핫키/리스폰)
 *   - 정체 시 루멘 힌트
 *   - 최초 경험 코치마크
 * 가 전부 이 한 곳을 바라보게 한다.
 *
 * 월드 서브시스템이 아니라 GameInstance 서브시스템인 이유:
 * 레벨 전환 중(월드가 없는 순간)에도 로딩 화면이 목표를 읽어야 하기 때문.
 */
UCLASS()
class RETRIEVE_API URetrieveGuidanceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── 현재 목표 캐시 ────────────────────────────────────────────────────────
	/** QuestTrackerViewModel이 목표를 재계산할 때마다 호출한다. */
	void UpdateTrackedObjective(const FText& InQuestName, const FText& InObjectiveText, FGameplayTag InStepTag);

	UFUNCTION(BlueprintPure, Category = "Retrieve|Guidance")
	FText GetQuestName() const { return QuestName; }

	UFUNCTION(BlueprintPure, Category = "Retrieve|Guidance")
	FText GetObjectiveText() const { return ObjectiveText; }

	UFUNCTION(BlueprintPure, Category = "Retrieve|Guidance")
	bool HasObjective() const { return !ObjectiveText.IsEmptyOrWhitespace(); }

	/** 로딩 화면/미니맵에 그대로 쓸 수 있는 한 줄. 목표가 없으면 빈 텍스트. */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Guidance")
	FText GetObjectiveBriefLine() const;

	// ── 목표 재확인 ───────────────────────────────────────────────────────────
	/**
	 * "내가 뭘 해야 하지?"에 대한 자가 복구 수단.
	 * Channel.UI.ObjectiveReminder를 쏘면 트래커가 강조되고 목표 마커가 다시 튀어오른다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Guidance")
	void RequestObjectiveReminder();

	// ── 최초 경험 코치마크 ────────────────────────────────────────────────────
	/**
	 * 같은 키로 처음 호출될 때만 DT_SystemMessage의 해당 배치를 띄운다.
	 * (전투 첫 조우, 첫 채집, 첫 모닥불 등 — 볼륨을 배치하지 않아도 동작한다)
	 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Guidance")
	void TriggerFirstTimeCoach(FGameplayTag KeyTag);

	/** 새 게임 시작 시 브리핑을 띄우고 코치마크 이력을 초기화한다. */
	void PlayIntroBriefing();
	void ResetForNewGame();

	// ── 힌트 ──────────────────────────────────────────────────────────────────
	/** 목표 진전이 이 시간(초) 동안 없으면 루멘이 힌트를 던진다. 0 이하면 끔. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Guidance")
	float HintIdleSeconds = 120.0f;

	/** 힌트를 한 번 던진 뒤 다음 힌트까지의 최소 간격(초). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Guidance")
	float HintRepeatSeconds = 90.0f;

	/** 사망 후 리스폰했을 때 목표를 다시 알려주기까지의 지연(초). 0 이하면 끔. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Guidance")
	float RespawnReminderDelay = 2.5f;

private:
	/** 1초마다 돌며 정체 시간을 재고 힌트를 던진다. 월드가 없어도 도는 코어 티커를 쓴다. */
	bool TickGuidance(float DeltaTime);

	/** 월드가 생기면 한 번만: 코치마크용 메시지 구독 + 새 게임이면 인트로 브리핑. */
	void EnsureWorldHooks(UWorld* World);

	void HandlePickupToast(FGameplayTag Channel, const struct FRetrievePickupToastPayload& Message);
	void HandleRested(FGameplayTag Channel, const struct FRetrievePlayerRestedPayload& Message);
	void HandlePlayerDied(FGameplayTag Channel, const struct FPlayerDiedPayload& Message);

	/** 이 월드에서 훅을 이미 걸었는지. 레벨이 바뀌면 다시 건다. */
	TWeakObjectPtr<UWorld> HookedWorld;

	FGameplayMessageListenerHandle PickupHandle;
	FGameplayMessageListenerHandle RestedHandle;
	FGameplayMessageListenerHandle DiedHandle;

	FTimerHandle RespawnReminderTimer;

	void FireHint();

	UWorld* GetActiveWorld() const;

	FText QuestName;
	FText ObjectiveText;
	FGameplayTag StepTag;

	/** 목표가 마지막으로 바뀐 시각(FPlatformTime::Seconds). 정체 판정 기준. */
	double LastObjectiveChangeTime = 0.0;
	double LastHintTime = -1000.0;

	/** 이미 띄운 코치마크 키. 세이브에는 남기지 않는다(세션 단위). */
	TSet<FGameplayTag> FiredCoachKeys;

	FTSTicker::FDelegateHandle TickerHandle;
};
