#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"

#include "GameplayTags/RetrieveGameplayTags.h"
#include "GameplayTagContainer.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "RetrieveDataTableTypes.generated.h"

class UStateTree;
class UCameraShakeBase;
class UStateTree;

USTRUCT(BlueprintType)
struct RETRIEVE_API FCharacterStats : public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Stats")
	float MaxHealth = 100.0f;
};

USTRUCT(BlueprintType)
struct RETRIEVE_API FCombatTimingRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Timing", meta=(ClampMin="0.0"))
	float Duration = 1.f;
};

USTRUCT(BlueprintType)
struct RETRIEVE_API FEnemyDropRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Drop")
	FGameplayTag ItemTag;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Drop", meta=(ClampMin="0.0", ClampMax="1.0"))
	float DropChance = 1.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Drop", meta=(ClampMin="1"))
	int32 Quantity = 1;
};

USTRUCT(BlueprintType)
struct RETRIEVE_API FMonsterPatternRow : public FTableRowBase
{
    GENERATED_BODY()

    /** 패턴 유형. Pattern.Type.Melee / Ranged / Special / PhaseTransition */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern")
    FGameplayTag PatternType;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern", meta=(ClampMin="0.0"))
    float ActivationRange = 200.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern", meta=(ClampMin="0.0"))
    float Cooldown = 3.f;

    /** 선택 우선순위. 높을수록 먼저 시도. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern")
    int32 Priority = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern")
    FName AnimationRow;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern")
    FGameplayTag EffectTag;
	
	/** 카운터 관련 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Counter")
    bool bCanBeParried = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Counter")
    bool bCanTriggerGroggy = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Counter")
    FGameplayTag CounterEventTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Counter")
    FGameplayTag RequiredElementTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Counter")
    FGameplayTag RequiredActionTag;
};

USTRUCT(BlueprintType)
struct RETRIEVE_API FMonsterDataRow : public FTableRowBase
{
	GENERATED_BODY()

	/** DT_CharacterStats의 Row 키. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Stats")
	FName StatsRow;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Stats")
	FGameplayTag MonsterType;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Stats")
	FGameplayTag ElementTag;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern")
	TArray<FName> PatternSlots;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Groggy", meta=(ClampMin="0.0"))
	float GroggyDuration = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Groggy", meta=(ClampMin="0.0"))
	float GroggyCooldown = 10.f;

	/** DT_EnemyDrop의 Row 키. 비어있으면 드랍 없음. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Drop")
	FName DropRow;
};

USTRUCT(BlueprintType)
struct RETRIEVE_API FBossStatsRow : public FTableRowBase
{
    GENERATED_BODY()

    /** DT_CharacterStats의 Row 키. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss|Stats")
    FName StatsRow;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss|Phase", meta=(ClampMin="1", ClampMax="3"))
    int32 PhaseCount = 1;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss|Phase", meta=(ClampMin="0.0", ClampMax="1.0"))
    float Phase2HPThreshold = 0.5f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss|Phase", meta=(ClampMin="0.0", ClampMax="1.0"))
    float Phase3HPThreshold = 0.25f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss|Groggy", meta=(ClampMin="0.0"))
    float GroggyDuration = 5.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss|Groggy", meta=(ClampMin="0.0"))
    float GroggyCooldown = 15.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss|Drop")
    FGameplayTag UnlockElementTag;
};

USTRUCT(BlueprintType)
struct RETRIEVE_API FEnemyListRow : public FTableRowBase
{
    GENERATED_BODY()

    /** DT_MonsterData의 Row 키. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemy|Spawn")
    FName MonsterDataRow;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemy|Spawn")
    TSoftObjectPtr<UStateTree> AIStateTree;

    /** DT_EnemyDrop의 Row 키. 이 구역 특화 드랍 재정의. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemy|Spawn")
    FName DropRow;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemy|Spawn")
    FGameplayTag SectionTag;
};

/**
 * 히트 강도별 피드백 설정
 */
USTRUCT(BlueprintType)
struct RETRIEVE_API FHitFeedback : public FTableRowBase
{
	GENERATED_BODY()
	
	// 카메라 흔들림 클래스
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	TSoftClassPtr<UCameraShakeBase> CameraShake;
	// 흔들림 강도 배수(1.0 == 기본)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "3.0"))
	float CameraShakeScale = 1.0f;
	// 매칭 되는 GameplayEvent 태그
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX", meta = (Categories = "GameplayEvent"))
	FGameplayTagContainer FXTags;
	// 대미지 숫자 플로터 크기 배수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Number", meta = (ClampMin = "0.5", UIMin = "0.5", UIMax = "3.0"))
	float DamageNumberScale = 1.0f;
	// 대미지 숫자 색상(강도별 시각 차별)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Number")
	FLinearColor DamageNumberColor = FLinearColor::White;
};

/**
 * 스킬 조합
 */
USTRUCT(BlueprintType)
struct RETRIEVE_API FSkillCombination : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Element")
	TMap<FGameplayTag, int32> ElementPattern;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion")
	FName MotionGroup;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	FGameplayTag PrimaryEffect;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	FGameplayTag SecondaryEffect;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	float DamageMultiplier;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	float EffectDuration;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	float AoeRadius;
};
