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

	// 버스트를 결정한 현재 원소모드 (Element.Fire/Water/Wind)
	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|ElementGauge")
	FGameplayTag BurstElement;
};

USTRUCT(BlueprintType)
struct FRetrieveElementGaugeFullPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|ElementGauge")
	TObjectPtr<AActor> Instigator = nullptr;

	// 게이지가 가득 찼을 때의 현재 원소모드 (Element.Fire/Water/Wind)
	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|ElementGauge")
	FGameplayTag Element;
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

// ---- 활 차징 ------------------------------------------------------
UENUM(BlueprintType)
enum class ERetrieveBowChargePhase : uint8
{
	Started, // 차징 시작 (조준 유지 + 좌 클릭 홀드 진입)
	Cancelled, // 조준 해제 등으로 차징 취소 (발사 없음)
	Released // 발사
};

/** Channel.Bow.Charge 로컬 페이로드. GA_BowShot이 차징 상태 변화를 발행, WBP_Reticle이 구독. */
USTRUCT(BlueprintType)
struct FRetrieveBowChargePayload
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Bow")
	TObjectPtr<AActor> Instigator = nullptr;
	
	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Bow")
	ERetrieveBowChargePhase Phase = ERetrieveBowChargePhase::Started;
	
	// 풀 차징까지 걸리는 시간 초, 위젯이 0 -> 1 애님 길이로 사용
	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Bow")
	float MaxChargeTime = 0.f;
	
	// Released 시점의 실제 차징 비율(0~1), 발사 피드백 용
	float ChargeRatio = 0.f;
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

// ---- UI 스킬 발동 팝업 ---------------------------------------------------

/** Channel.UI.SkillActivated 페이로드.
 *  GA_Burst가 스킬 조합 매칭 직후 브로드캐스트.
 *  WBP_BurstSkillPopup이 구독해 화면 중앙에 스킬 이름을 잠깐 표시한다.
 */
USTRUCT(BlueprintType)
struct FRetrieveUISkillActivatedPayload
{
	GENERATED_BODY()

	/** FSkillCombination::DisplayName — 화면에 표시할 스킬 이름 */
	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|UI|Skill")
	FText SkillName;

	/** FSkillCombination::BurstUITag — 버프바 Row 조회 키 (아이콘 재사용 가능) */
	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|UI|Skill")
	FGameplayTag UITag;
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

	/** true이면 BuffBar가 같은 BuffId를 중첩 카운트로 처리한다 */
	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|UI|Buff")
	bool bIsStackable = false;

	/** 스택 최대치. 0이면 무제한. bIsStackable이 true일 때만 유효. */
	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|UI|Buff")
	int32 MaxStack = 0;

	/** GE가 보고한 실제 스택 수. 0이면 정보 없음 → BuffBar가 Apply 횟수로 카운트(기존 동작 유지). */
	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|UI|Buff")
	int32 StackCount = 0;
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

// ---- 화톳불 / 휴식 --------------------------------------------------

/** Channel.Player.Rested 페이로드. 플레이어가 화톳불을 사용할 때 브로드캐스트. */
USTRUCT(BlueprintType)
struct FRetrievePlayerRestedPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Rest")
	TObjectPtr<AActor> Instigator;

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Rest")
	FName BonfireId;
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

// ---- Bark / 시스템 메시지 ------------------------------------------------

/** Bark 트리거 소스. AmbientRandom = 근접 풀, OnQuestStep = 스텝 완료 직후, Manual = 볼륨/훅/치트. */
UENUM(BlueprintType)
enum class EBarkTrigger : uint8
{
	AmbientRandom,
	OnQuestStep,
	Manual
};

/** Channel.UI.BarkRequested 로컬 페이로드. UBarkSubsystem이 행에서 한 줄을 골라 채워 발행합니다. */
USTRUCT(BlueprintType)
struct FRetrieveBarkPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Bark")
	FGameplayTag SpeakerTag;

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Bark")
	FText SpeakerName;

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Bark")
	FText Line;

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Bark")
	float Duration = 4.0f;

	/** (선택) Bark 메세지 자막이 뜰 때 재생할 오디오. */
	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Bark")
	TSoftObjectPtr<USoundBase> Cue;
};

/** Channel.UI.SystemMessage 로컬 페이로드. 튜토리얼/정보 표시용 (우상단 텍스트). */
USTRUCT(BlueprintType)
struct FRetrieveSystemMessagePayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|UI|System")
	FText Text;

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|UI|System")
	float Duration = 4.0f;
};

/** Channel.UI.DialogueChanged 로컬 페이로드. PC가 대화 열기(true)/닫기(false)에 발행. Bark를 막는 신호. */
USTRUCT(BlueprintType)
struct FRetrieveDialogueChangedPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|UI|Dialogue")
	bool bActive = false;
};
