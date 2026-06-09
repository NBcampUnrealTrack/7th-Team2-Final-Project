#pragma once

#include "CoreMinimal.h"
#include "Core/RetrieveSessionState.h"
#include "GameplayTagContainer.h"
#include "RetrieveMessageTypes.generated.h"

class UTexture2D;
class UGameplayEffect;

// ---- 세션 ---------------------------------------------------------

USTRUCT(BlueprintType)
struct FRetrieveSessionStatePayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Session")
	ERetrieveSessionState PreviousState = ERetrieveSessionState::Loading;

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Session")
	ERetrieveSessionState NewState = ERetrieveSessionState::Loading;
};

// ---- 전투 ---------------------------------------------------------

USTRUCT(BlueprintType)
struct FRetrieveItemElementBuffPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|ElementBuff")
	TObjectPtr<AActor> Instigator = nullptr;

	// 아이템이 지정한 원소 (Element.Fire/Water/Wind)
	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|ElementBuff")
	FGameplayTag ElementTag;

	// 원소 충전 배율 (기본 1.0, 버프 아이템은 >1.0)
	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|ElementBuff")
	float Multiplier = 1.0f;

	// 버프 지속 시간(초). 0이면 즉시 소멸
	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|ElementBuff")
	float Duration = 0.0f;
};

USTRUCT(BlueprintType)
struct FRetrieveElementGaugeBurstPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|ElementGauge")
	TObjectPtr<AActor> Instigator = nullptr;

	// 버스트 발동에 쓰인 원소 조합 (Tag → 슬롯 개수)
	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|ElementGauge")
	TMap<FGameplayTag, int32> ElementPattern;
};

USTRUCT(BlueprintType)
struct FRetrieveElementGaugeFullPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|ElementGauge")
	TObjectPtr<AActor> Instigator = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|ElementGauge")
	TArray<FGameplayTag> FilledElements;
};

/** Channel.Combat.DamageDealt 페이로드 공격자 측 연출(대미지 숫자 플로터 등)에 사용
 * CombatAttributeSet::BroadcastEvent에서 발행
 */
USTRUCT(BlueprintType)
struct FRetrieveDamageDealtPayload
{
	GENERATED_BODY()

	// 대미지를 가한 액터(자기판정용 - 본인이 가한 공격만 연출)
	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Combat")
	TObjectPtr<AActor> Instigator = nullptr;
	// 피격 액터(플로터 표시 위치 기준)
	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Combat")
	TObjectPtr<AActor> Target = nullptr;
	// 입힌 대미지량
	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Combat")
	float DamageAmount = 0.f;
	// 강도 태그(GameplayEvent.Attack.HitSuccess.Light/Heavy) - DT_HitFeedback 조회 키
	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Combat")
	FGameplayTag HitEventTag;
	// 피격 강도 태그(GameplayEvent.Hit.Normal/.Heavy) - 피격 측 흔들림 조회 키
	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Combat")
	FGameplayTag TargetEventTag;
};

// ---- 루멘 ---------------------------------------------------------

/** 루멘 Follow/Positioning 모드. 호스트만 쓸 수 있습니다. ULumenFollowComponent에서 Replicate 됩니다. */
UENUM(BlueprintType)
enum class EFollowMode : uint8
{
	Follow,
	Wait,
	RetreatCombat
};

/** OnRep_Mode마다 Channel.Lumen.Mode.Changed로 브로드캐스트됩니다. */
USTRUCT(BlueprintType)
struct FRetrieveLumenModePayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Lumen")
	EFollowMode Mode = EFollowMode::Follow;
};

/** Channel.Lumen.Command.ToggleWait와 Channel.Lumen.Command.Recall의 공유 페이로드. */
USTRUCT(BlueprintType)
struct FRetrieveLumenCommandPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Lumen")
	FGameplayTag CommandTag;

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Lumen")
	TObjectPtr<AActor> Instigator = nullptr;
};

// ---- 퀘스트 -------------------------------------------------------------------

/** Channel.Quest.StepChanged / Channel.Quest.SealUnlocked 페이로드. */
USTRUCT(BlueprintType)
struct FRetrieveQuestStepPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Quest")
	FGameplayTag StepTag;
};

// ---- UI 버프/디버프 바 ---------------------------------------------------

USTRUCT(BlueprintType)
struct FRetrieveUIBuffPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|UI|Buff")
	FGameplayTag BuffId;

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|UI|Buff")
	FText DisplayName;

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|UI|Buff")
	FText Description;

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|UI|Buff")
	FText EffectSummary;

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|UI|Buff")
	TSubclassOf<UGameplayEffect> LinkedGameplayEffect;

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|UI|Buff")
	TObjectPtr<UTexture2D> Icon = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|UI|Buff")
	FLinearColor TintColor = FLinearColor::White;

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|UI|Buff")
	float Duration = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|UI|Buff")
	bool bIsDebuff = false;
};

USTRUCT(BlueprintType)
struct FRetrieveUIBuffRemovePayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|UI|Buff")
	FGameplayTag BuffId;
};

/** Channel.Quest.GuardianDefeated 페이로드. */
USTRUCT(BlueprintType)
struct FRetrieveGuardianDefeatedPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Quest")
	FGameplayTag GuardianElement;
};

// ---- 대화 / 시네마틱 --------------------------------------------------

/** 토픽 종류. Story = 대사 라인들, Command = Lumen 커맨드, Sigil = 대화+VFX */
UENUM(BlueprintType)
enum class ETopicKind : uint8
{
	Story,
	Command,
	Sigil
};

/** 선택 가능한 토픽 하나. 유효하지 않은 TopicId는 대화 종료. */
USTRUCT(BlueprintType)
struct FRetrieveDialogueTopic
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Dialogue")
	FGameplayTag TopicId;

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Dialogue")
	FText Label;

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Dialogue")
	bool bEnabled = true;

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Dialogue")
	ETopicKind Kind = ETopicKind::Story;
};

/** GameState의 Replicated된 대화 상태. 비트 = Lines[] (한 번에 하나씩 재생; Enter/클릭으로 로컬 진행) → Topics[] (마지막 대사 이후 표시). */
USTRUCT(BlueprintType)
struct FRetrieveDialogueState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Dialogue")
	FText SpeakerName;

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Dialogue")
	TArray<FText> Lines;

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Dialogue")
	TArray<FRetrieveDialogueTopic> Topics;

	/** 호스트 권한 대화. 모두가 대사 + VFX를 볼 수 있지만, 진행은 호스트만 가능합니다. */
	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Dialogue")
	bool bSharedNarrative = true;

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Dialogue")
	bool bHoldUntilReplaced = false;

	/** RequestDialogue/ClearDialogue 호출마다 증가. 콘텐츠가 동일해도 OnRep이 발생하도록 함. */
	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Dialogue")
	int32 Serial = 0;
};

/** Channel.Dialogue.LineRequested 로컬 페이로드. */
USTRUCT(BlueprintType)
struct FRetrieveDialoguePayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Dialogue")
	FText SpeakerName;

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Dialogue")
	TArray<FText> Lines;

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Dialogue")
	TArray<FRetrieveDialogueTopic> Topics;

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Dialogue")
	bool bSharedNarrative = true;

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Dialogue")
	bool bHoldUntilReplaced = false;
};

/** 최소한의 시네마틱 상태. */
USTRUCT(BlueprintType)
struct FRetrieveCinematicState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Cinematic")
	bool bActive = false;

	bool IsActive() const { return bActive; }
};

/** Channel.Cinematic.Changed 로컬 페이로드. */
USTRUCT(BlueprintType)
struct FRetrieveCinematicStatePayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Cinematic")
	bool bActive = false;
};
