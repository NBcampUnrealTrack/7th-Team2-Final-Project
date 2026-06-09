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
