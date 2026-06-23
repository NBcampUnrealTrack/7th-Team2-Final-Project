#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"

#include "GameplayTagContainer.h"
#include "Character/Cosmetics/RetrieveModularMeshTypes.h"
#include "Combat/RetrieveCombatTypes.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "RetrieveDataTableTypes.generated.h"

class UStateTree;
class UCameraShakeBase;
class UAnimInstance;
class UAnimMontage;
class UAnimSequenceBase;
class UGameplayEffect;
class URetrieveAbilitySet;
class UAttackComboDefinition;
class USkeletalMesh;
class UStaticMesh;
class UTexture2D;
class UNiagaraSystem;
class USoundBase;
class AStaffProjectile;
class AEnemyProjectile;

// FGenericTeamId(uint8)에 매핑되는 게임 정의 팀 식별자.
// 엔진은 NoTeam(255)만 예약하고 팀 의미는 게임이 정의하도록 둠 → 여기서 정의한다.
UENUM(BlueprintType)
enum class ERetrieveTeam : uint8
{
	Neutral = 0,
	Player  = 1,
	Enemy   = 2,
};

USTRUCT(BlueprintType)
struct RETRIEVE_API FCharacterStats : public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Stats")
	float MaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Stats")
	float AttackPower = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Stats")
	float Defense = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Stats")
	float MoveSpeed = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Stats")
	float IncomingDamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Stats", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GuardDamageReduction = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Stats", meta = (ClampMin = "0.1"))
	float AttackSpeedMultiplier = 1.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Stats|Stamina")
	float MaxStamina = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Stats|Stamina")
	float StaminaRegenRate = 50.0f;
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

	/** 인벤토리에 지급할 아이템 ID. 무기/소모품/재료 DataTable의 RowName(==ItemId)과 일치시킨다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Drop")
	FName ItemId;

	/** 아이템 분류 태그. Item.Weapon / Item.Consumable / Item.Material 등. 인벤토리 카테고리 분기에 사용. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Drop", meta=(Categories="Item"))
	FGameplayTag ItemCategoryTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Drop", meta=(ClampMin="0.0", ClampMax="1.0"))
	float DropChance = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Drop", meta=(ClampMin="1"))
	int32 Quantity = 1;
};

/**
 * 투사체 스폰 방식. 새 패턴은 이 enum 값만 골라서 MonsterPattern row로 추가할 수 있다.
 * Aimed: 타겟을 직접 조준 발사 (기본, 일반/보스 호환)
 * RainFromAbove: 타겟 머리 위에서 아래로 떨어지는 비 패턴
 * RadialSpread: 몬스터 위치에서 360도 무작위 방향으로 확산 발사
 * GroundPillar: 타겟 발밑에서 위로 솟아오르는 기둥 패턴 (물기둥 등)
 */
UENUM(BlueprintType)
enum class EProjectileSpawnPattern : uint8
{
	Aimed			UMETA(DisplayName="Aimed"),
	RainFromAbove	UMETA(DisplayName="Rain From Above"),
	RadialSpread	UMETA(DisplayName="Radial Spread"),
	GroundPillar	UMETA(DisplayName="Ground Pillar"),
};

USTRUCT(BlueprintType)
struct RETRIEVE_API FMonsterProjectilePatternConfig
{
	GENERATED_BODY()

	/** 투사체 스폰 방식. 기본 Aimed는 기존 동작과 동일. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Projectile")
	EProjectileSpawnPattern SpawnPattern = EProjectileSpawnPattern::Aimed;

	/** 각 투사체 발사 시점. 비어 있으면 투사체 패턴 설정을 사용하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Projectile")
	TArray<float> ProjectileFireDelays;

	/** 투사체 속도 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Projectile", meta=(ClampMin="0.0"))
	float ProjectileSpeed = 1200.f;
	
	/** 투사체 생존 시간 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Projectile", meta=(ClampMin="0.0"))
	float ProjectileLifetime = 5.f;
	
	/** 유도 여부 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Projectile")
	bool bUseHoming = false;

	/** 발사 후 유도 시작까지의 지연 시간 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Projectile", meta=(ClampMin="0.0"))
	float HomingStartDelay = 0.f;

	/** 유도 지속 시간 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Projectile", meta=(ClampMin="0.0"))
	float HomingDuration = 0.f;

	/** 유도 강도 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Projectile", meta=(ClampMin="0.0"))
	float HomingStrength = 0.f;
	
	/** 중력 적용 여부 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Projectile")
	bool bUseGravity = false;

	/** 중력 강도 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Projectile", meta=(ClampMin="0.0"))
	float ProjectileGravityScale = 0.f;

	// ---- RainFromAbove 파라미터 ----
	/** 타겟 기준 낙하 시작 높이 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Projectile|Rain", meta=(ClampMin="0.0"))
	float RainSpawnHeight = 850.f;

	/** 타겟 주변 수평 무작위 반경 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Projectile|Rain", meta=(ClampMin="0.0"))
	float RainSpawnRadius = 260.f;

	// ---- GroundPillar 파라미터 ----
	/** 타겟 기준 기둥 스폰 깊이. 양수 값만 입력하고 런타임에서 아래 방향으로 적용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Projectile|GroundPillar", meta=(ClampMin="0.0"))
	float GroundPillarSpawnDepth = 80.f;

	// ---- RadialSpread 파라미터 ----
	/** 한 번의 확산 스폰에서 생성할 투사체 수 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Projectile|Spread", meta=(ClampMin="1"))
	int32 RadialProjectileCount = 8;

	/** 확산 발사 시 피치 최소각(deg) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Projectile|Spread")
	float SpreadPitchMin = -15.f;

	/** 확산 발사 시 피치 최대각(deg) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Projectile|Spread")
	float SpreadPitchMax = 20.f;

	/** 확산 첫 투사체 속도 배율 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Projectile|Spread", meta=(ClampMin="0.0"))
	float SpreadSpeedMultiplierMin = 0.9f;

	/** 확산 마지막 투사체 속도 배율 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Projectile|Spread", meta=(ClampMin="0.0"))
	float SpreadSpeedMultiplierMax = 1.2f;

	// ---- 반사(패링 카운터) 설정 — 에픽 투사체에서만 사용 ----
	/** 플레이어 패링 시 카운터 타겟으로 반사되는 투사체인지 여부 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Projectile|Reflection")
	bool bReflectable = false;

	/** 반사 시 속도 배율 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Projectile|Reflection", meta=(ClampMin="0.1"))
	float ReflectedSpeedMultiplier = 1.2f;
};

USTRUCT(BlueprintType)
struct RETRIEVE_API FMonsterSwordBarrageConfig
{
	GENERATED_BODY()

	/** 소환할 검(투사체) 개수 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SwordBarrage", meta=(ClampMin="1", ClampMax="15"))
	int32 SwordCount = 5;

	/** 검이 배치되는 호(arc)의 반지름 - 보스 중심에서의 거리 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SwordBarrage", meta=(ClampMin="0.0"))
	float ArcRadius = 250.f;

	/** 검이 퍼지는 호의 각도 (180 = 반원) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SwordBarrage", meta=(ClampMin="1.0", ClampMax="360.0"))
	float ArcAngleDegrees = 180.f;

	/** 호 중심의 높이 오프셋 - 보스 기준 위로 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SwordBarrage")
	float HeightOffset = 130.f;

	/** 호 중심의 전후 오프셋 — 보스 기준, 음수면 뒤쪽 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SwordBarrage")
	float ForwardOffset = -80.f;

	/** 어빌리티 시작 후 검 소환까지 대기 시간(초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SwordBarrage", meta=(ClampMin="0.0"))
	float SummonDelay = 0.5f;

	/** 검 소환 후 발사까지 대기 시간(초) - 검이 떠 있는 예고 구간 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SwordBarrage", meta=(ClampMin="0.0"))
	float LaunchDelay = 1.f;

	/** 발사된 검의 이동 속도 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SwordBarrage", meta=(ClampMin="1.0"))
	float ProjectileSpeed = 1600.f;

	/** 발사된 검의 생존 시간(초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SwordBarrage", meta=(ClampMin="0.1"))
	float ProjectileLifetime = 6.f;

	/** 조준점 오프셋 - 타겟 위치 기준 (예: Z+로 몸통 높이 조준) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SwordBarrage")
	FVector TargetOffset = FVector(0.f, 0.f, 70.f);
};

USTRUCT(BlueprintType)
struct RETRIEVE_API FMonsterAerialDiveConfig
{
	GENERATED_BODY()

	/** 상승 목표 높이 - 보스 현재 위치 기준 위로 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AerialDive", meta=(ClampMin="0.0"))
	float TakeoffHeight = 650.f;

	/** 상승 속도 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AerialDive", meta=(ClampMin="0.0"))
	float RiseSpeed = 900.f;

	/** 정점에서 조준 유지 시간(초) - 이 동안 타겟 추적 후 방향 락 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AerialDive", meta=(ClampMin="0.0"))
	float AimDuration = 0.7f;

	/** 급강하 속도 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AerialDive", meta=(ClampMin="0.0"))
	float DiveSpeed = 2800.f;

	/** 단계 전환 도달 판정 허용 오차 - 상승 완료/돌진 완료 거리 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AerialDive", meta=(ClampMin="0.0"))
	float PositionTolerance = 25.f;

	/** 전체 어빌리티 안전 타임아웃(초) - 어떤 단계든 이 시간 초과 시 강제 종료 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AerialDive", meta=(ClampMin="0.0"))
	float AbilityTimeout = 7.f;

	/** 돌진 목표점 오프셋 - 타겟 위치 기준 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AerialDive")
	FVector TargetOffset = FVector(0.f, 0.f, 0.f);

	/** 타겟 앞에서 멈추는 수평 거리 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AerialDive", meta=(ClampMin="0.0"))
	float DiveStopDistance = 80.f;
};

USTRUCT(BlueprintType)
struct RETRIEVE_API FMonsterLaunchKnockbackConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Knockback")
	bool bUseLaunchKnockback = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Knockback", meta=(ClampMin="0.0"))
	float KnockbackStrength = 800.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Knockback", meta=(ClampMin="0.0"))
	float KnockbackUpwardStrength = 400.f;
};

USTRUCT(BlueprintType)
struct RETRIEVE_API FMonsterPatternRow : public FTableRowBase
{
    GENERATED_BODY()

    /** 패턴 유형. Pattern.Type.Melee / Ranged / Special / PhaseTransition */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern")
    FGameplayTag PatternType;
	
	/** 이 패턴을 실행할 GameplayEvent. 비어있으면 패턴 유형별 기본 이벤트를 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern")
	FGameplayTag AbilityEventTag;
	
	/** 발동 최대 거리 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern", meta=(ClampMin="0.0"))
	float MaxActivationRange = 200.f;
	
	/** 발동 최소 거리 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern", meta=(ClampMin="0.0"))
	float MinActivationRange = 0.f;

	/** 패턴 쿨타임 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern", meta=(ClampMin="0.0"))
    float Cooldown = 3.f;

	/** 파훼 시 그로기 지속 시간 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Groggy", meta=(ClampMin="0.0"))
	float GroggyDuration = 3.f;
	
    /** 선택 우선순위. 높을수록 먼저 시도. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern")
    int32 Priority = 0;
	
	/** 재생할 애니메이션 몽타주. 없으면 AttackSequence를 동적 몽타주로 재생한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern")
	TSoftObjectPtr<UAnimMontage> AttackMontage;

	/** AttackMontage가 없을 때 동적 몽타주로 재생할 AnimSequence */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern")
	TSoftObjectPtr<UAnimSequenceBase> AttackSequence;

	/** 히트박스 활성화 시작 시간 (초). 0이면 즉시 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Hitbox", meta=(ClampMin="0.0"))
	float HitboxWindowStartTime = 0.f;

	/** 히트박스 활성화 지속 시간 (초). 0이면 히트박스 타이머 없음 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Hitbox", meta=(ClampMin="0.0"))
	float HitboxWindowDuration = 0.f;

	/** 투사체 패턴 설정. 투사체를 사용하지 않는 패턴은 기본값을 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Projectile")
	FMonsterProjectilePatternConfig ProjectileConfig;

	/** BossQueen의 SwordBarrage 패턴 설정 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|SwordBarrage")
	FMonsterSwordBarrageConfig SwordBarrageConfig;

	/** BossQueen의 AerialDive 패턴 설정 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|AerialDive")
	FMonsterAerialDiveConfig AerialDiveConfig;

	/** 이 패턴에서 사용할 투사체 클래스. 비어 있으면 GA의 기본 ProjectileClass를 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Projectile")
	TSubclassOf<AEnemyProjectile> ProjectileClass;
	
	/** 피격 시 적용할 효과 태그 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern")
    FGameplayTag EffectTag;

	/** 투사체 적중 또는 범위 충돌 시 적용할 상태이상 GE. 비어 있으면 상태이상을 적용하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Status")
	TSubclassOf<UGameplayEffect> StatusEffectClass;

	/** 적중 시 피격자 반응 강도 (방어판정/데미지와 독립) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|HitReact")
	ERetrieveHitReactType HitReactType = ERetrieveHitReactType::Flinch;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Knockback")
	FMonsterLaunchKnockbackConfig LaunchKnockbackConfig;

	/** 카운터 관련 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Counter")
    bool bCanBeParried = false;

	/** 파훼 시 Gorggy 트기거 작동 여부 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Counter")
    bool bCanTriggerGroggy = false;

	/** 파훼 성공 시 발생시킬 이벤트 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Counter")
    FGameplayTag CounterEventTag;

	/** 파훼에 필요한 원소 모드. 없으면 None  */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Counter")
    FGameplayTag RequiredElementTag;

	/** 파훼에 필요한 행동. 패링 / 회피 / 강공격 등 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Counter")
    FGameplayTag RequiredActionTag;
	
	/** 피격 판정 HitBox가 부착될 Bone 이름 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Hitbox")
	FName HitboxBoneName = NAME_None;

	/** 피격 판정 HitBox의 반지름 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Hitbox", meta=(ClampMin="0.0"))
	float HitboxRadius = 30.f;

	/** 피격 판정 HitBox의 본의로부터의 Offset */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Hitbox")
	FVector HitboxOffset = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct RETRIEVE_API FMonsterDataRow : public FTableRowBase
{
	GENERATED_BODY()

	/** DT_CharacterStats의 Row 키. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Stats")
	FName StatsRow;

	/** 일반 / 에픽 / 보스 구분 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Stats")
	FGameplayTag MonsterType;

	/** 넉백 면역. 보스는 MonsterType으로 자동 면역되며, 에픽/일반은 이 플래그로 디자이너가 제어. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Knockback")
	bool bKnockbackImmune = false;

	/** 몬스터 또는 보스가 사용하는 주요 원소 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Stats")
	FGameplayTag ElementTag;
	
	/** Monster가 보유한 공격 패턴들의 Row 이름. DT_MonsterPatternRow의 Row 이름. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern")
	TArray<FName> PatternSlots;

	/** 그로기 종료 후 재진입 대기 시간 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Groggy", meta=(ClampMin="0.0"))
	float GroggyCooldown = 10.f;

	/** 범위 지정 - 공격 가능 범위 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster|Attack")
	float AttackableRange = 200.f;
	
	/** 범위 지정 - Strafe 진입 범위 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moster|Move")
	float StrafeOffRange = 360.f;
	
	/** Strafe 위치 지정 시 최소 노이즈 값 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moster|Move")
	float StrafeMinNoise = -100.f;
	
	/** Strafe 위치 지정 시 최대 노이즈 값 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moster|Move")
	float StrafeMaxNoise = 10.f;

	/** 공격권 비용. 기본 1, 큰 몬스터는 2 이상 권장 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster|Attack", meta=(ClampMin="1"))
	int32 AttackTokenCost = 1;

	/** 타겟 주변 공격권 총 예산. 0 이하면 시스템 기본값 사용 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster|Attack", meta=(ClampMin="0"))
	int32 AttackTokenBudget = 0;

	/** 공격권 보유 몬스터가 사용할 안쪽 배회 반경 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moster|Move", meta=(ClampMin="0.0"))
	float OrbitInnerRadius = 160.f;

	/** 공격권이 없는 몬스터가 사용할 바깥 배회 반경 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moster|Move", meta=(ClampMin="0.0"))
	float OrbitOuterRadius = 300.f;
	
	/** 범위 지정 - 추적 가능 범위 */
	UPROPERTY(EditAnywhere, Category = "Moster|Move")
	float ChaseRange = 1500.f;
	
	/** Return -> Chase 가능해지는 스폰 지점과의 거리 */
	UPROPERTY(EditAnywhere, Category = "Moster|Move")
	float RechasableRange = 100.f;
	
	/** Patrol 범위 */
	UPROPERTY(EditAnywhere, Category = "Moster|Move")
	float PatrolRange = 1200.f;
	
	/** 이동 오차 허용 범위 */
	UPROPERTY(EditAnywhere, Category = "Moster|Move")
	float MoveAcceptableRadius = 5.f;
	/** 순찰 여부 */
	UPROPERTY(EditAnywhere, Category = "Moster|Move")
	bool bPatrolable = false;

	/** 에픽 몬스터 공중 페이즈 사용 여부 (비행 가능 몬스터만 true) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Monster|Epic")
	bool bHasAerialPhase = false;

	/** DT_EnemyDrop의 Row 키 목록. 각 행을 DropChance로 독립 굴림해 드랍한다. 비어있으면 드랍 없음. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Drop")
	TArray<FName> DropRows;
};

USTRUCT(BlueprintType)
struct RETRIEVE_API FBossPhaseTransitionMontageEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss|Phase", meta=(ClampMin="2", ClampMax="3"))
	int32 TargetPhase = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss|Phase")
	TSoftObjectPtr<UAnimMontage> Montage;
};

USTRUCT(BlueprintType)
struct RETRIEVE_API FBossStatsRow : public FTableRowBase
{
    GENERATED_BODY()

    /** DT_CharacterStats의 Row 키. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss|Stats")
    FName StatsRow;

	/** 보스 페이즈 수 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss|Phase", meta=(ClampMin="1", ClampMax="3"))
    int32 PhaseCount = 1;

	/** 2페이즈 진입 HP 비율  */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss|Phase", meta=(ClampMin="0.0", ClampMax="1.0"))
    float Phase2HPThreshold = 0.5f;

	/** 3페이즈 진입 HP 비율. 없으면 0 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss|Phase", meta=(ClampMin="0.0", ClampMax="1.0"))
    float Phase3HPThreshold = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss|Phase")
	TArray<FBossPhaseTransitionMontageEntry> PhaseTransitionMontages;

	/** 처치 시 해방할 원소 */
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

	/** 사용할 AI State Tree 에셋 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemy|Spawn")
    TSoftObjectPtr<UStateTree> AIStateTree;

    /** DT_EnemyDrop의 Row 키. 이 구역 특화 드랍 재정의. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemy|Spawn")
    FName DropRow;

	/** 등장 섹션 / 지역 태그 */
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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "20.0"))
	float CameraShakeScale = 1.0f;
	// 매칭 되는 GameplayEvent 태그
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit", meta = (Categories = "GameplayEvent"))
	FGameplayTagContainer HitEventTags;
	// 대미지 숫자 플로터 크기 배수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Number", meta = (ClampMin = "0.5", UIMin = "0.5", UIMax = "20.0"))
	float DamageNumberScale = 1.0f;
	// 대미지 숫자 색상(강도별 시각 차별)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Number")
	FLinearColor DamageNumberColor = FLinearColor::White;
	// 대미지 플로터 머리 위 오프셋(cm) — 강도별로 다르게 띄우려면 행에서 조절
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Number", meta = (ClampMin = "0.0"))
	float FloaterWorldZOffset = 90.f;
};

/**
 * 원소 게이지 충전 규칙. GameplayEvent 태그별로 충전량을 정의한다.
 */
USTRUCT(BlueprintType)
struct RETRIEVE_API FElementChargeRule : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gauge", meta=(Categories="GameplayEvent"))
	FGameplayTag EventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gauge", meta=(ClampMin="0"))
	int32 ChargeAmount = 0;
};

UENUM(BlueprintType)
enum class EBurstAttackType : uint8
{
	Cleave           UMETA(DisplayName = "단일 강타"),
	WorldActor       UMETA(DisplayName = "월드 액터 (소환물)"),
	Projectile       UMETA(DisplayName = "투사체"),
	Dash             UMETA(DisplayName = "돌진"),
	AreaOfEffect     UMETA(DisplayName = "주변 AoE")
};

UENUM(BlueprintType)
enum class EBurstHitSource : uint8
{
	Sword    UMETA(DisplayName = "검 (Weapon_R 계열)"),
	Shield   UMETA(DisplayName = "방패 (Shield 소켓 계열)"),
	Body     UMETA(DisplayName = "캐릭터 본체 (돌진/AoE)"),
	World    UMETA(DisplayName = "월드 좌표 (지면 분출)")
};

USTRUCT(BlueprintType)
struct RETRIEVE_API FBurstHitInstance
{
	GENERATED_BODY()

	/** 이 타격의 공격력 배율. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit")
	float DamageMultiplier = 1.0f;

	/** 어디서 나가는지. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit")
	EBurstHitSource HitSource = EBurstHitSource::Sword;

	/** 소켓 오버라이드. 비우면 HitSource 기본값 사용. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit")
	FName SocketOverride = NAME_None;

	/** 발사 시 검기 기울임 각도(도). Projectile 전용. 진행 방향은 정면 고정, 검기 자세만 진행축 기준 회전.
	 *  예: 3연격을 -30 / 0 / +30 으로 주면 부채살처럼 기울어진 검기. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit")
	float LaunchRollAngle = 0.f;

	/** 이 타격에서 재생할 적중 VFX. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit")
	TSoftObjectPtr<UNiagaraSystem> HitVFX;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit")
	TSoftObjectPtr<USoundBase> HitSound;

	/** 이 타격이 적중한 대상에 순차 부여할 상태 GE들. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit|Status")
	TArray<TSubclassOf<UGameplayEffect>> StatusEffects;

	/** 넉백 수평 강도(0이면 없음). 적중 시 공격자→피격자 방향으로 자동 적용. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit|Knockback", meta = (ClampMin = "0.0"))
	float KnockbackStrength = 0.f;

	/** 넉백 상향(Z) 강도. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit|Knockback", meta = (ClampMin = "0.0"))
	float KnockbackUpwardStrength = 0.f;
};

/**
 * 원소 반응 규칙. DT_ElementReaction 의 행.
 * 상태 GE 가 대상에 부여되는 시점에, 대상이 RequiredExistingTag 를 이미 보유하고 있으면
 * ReactionEffect 를 적용하고 RemoveStatusTag 상태를 제거한다.
 */
USTRUCT(BlueprintType)
struct RETRIEVE_API FElementReactionRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 이 상태 GE 가 대상에 부여될 때 반응을 검사한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction")
	TSubclassOf<UGameplayEffect> IncomingStatusEffect;

	/** 대상이 이 태그를 이미 보유 중이면 반응 발동. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction", meta = (Categories = "State.Status"))
	FGameplayTag RequiredExistingTag;

	/** 반응 시 적용할 효과 (추가 데미지 / 이속 디버프 등). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction")
	TSubclassOf<UGameplayEffect> ReactionEffect;

	/** 반응 시 제거할 상태 태그. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction", meta = (Categories = "State.Status"))
	FGameplayTag RemoveStatusTag;
};

/**
 * 스킬 조합
 */
USTRUCT(BlueprintType)
struct RETRIEVE_API FSkillCombination : public FTableRowBase
{
	GENERATED_BODY()

	// ---- Name --------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Meta")
	FText DisplayName;

	// ---- Pattern / Motion --------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Element", meta = (Categories = "Weapon.Type"))
	FGameplayTag WeaponType;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Element", meta = (Categories = "Element"))
	FGameplayTag BurstElement;
	// TODO(하민): [구버전] 정확 조합 매칭용. 원소 다수결 선택에서는 사용하지 않음(BurstElement로 대체). 추후 정리 예정
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Element")
	TMap<FGameplayTag, int32> ElementPattern;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Motion")
	TSoftObjectPtr<UAnimMontage> AttackMontage;
	// ---- Cast Lock --------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Motion")
	bool bLockMovementDuringCast = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Motion")
	bool bLockRotationDuringCast = true;
	// ---- Attack --------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Attack")
	EBurstAttackType AttackType = EBurstAttackType::Cleave;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Attack|Projectile", meta = (EditCondition = "AttackType == EBurstAttackType::Projectile"))
	TSubclassOf<AActor> ProjectileClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Attack|Dash", meta = (EditCondition = "AttackType == EBurstAttackType::Dash", ClampMin = "0.0"))
	float DashDistance = 0.f;
	/** DashDistance를 이동하는 데 걸리는 목표 시간. 발사 속도 = DashDistance / DashLaunchDuration. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Attack|Dash", meta = (EditCondition = "AttackType == EBurstAttackType::Dash", ClampMin = "0.01"))
	float DashLaunchDuration = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Attack|AoE", meta = (EditCondition = "AttackType == EBurstAttackType::AreaOfEffect", ClampMin = "0.0"))
	float AoeRadius = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Attack|World", meta = (EditCondition = "AttackType == EBurstAttackType::WorldActor", ClampMin = "0.0"))
	float WorldSpawnDistance = 300.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Attack|World", meta = (EditCondition = "AttackType == EBurstAttackType::WorldActor"))
	TSubclassOf<AActor> WorldSpawnActorClass;
	// ---- Damage --------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Damage")
	TSubclassOf<UGameplayEffect> DamageEffect;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Damage")
	TArray<FBurstHitInstance> HitSequence;
	// ---- FX --------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|FX")
	TSoftObjectPtr<USoundBase> CastSound;

	/** 버스트 발동 시 버프 바에 표시할 UI 태그. DT_BuffUIDefinitions Row Name과 일치시킨다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|FX")
	FGameplayTag BurstUITag;
};

USTRUCT(BlueprintType)
struct RETRIEVE_API FRetrieveItemStack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (Categories = "Item"))
	FGameplayTag ItemCategoryTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "0"))
	int32 Quantity = 1;
};

USTRUCT(BlueprintType)
struct RETRIEVE_API FRetrieveEquippedArmorEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armor", meta = (Categories = "Equipment.Slot"))
	FGameplayTag EquipmentSlotTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armor")
	FName ArmorItemId = NAME_None;
};

// SaveGame 복원용 인벤토리 스냅샷
USTRUCT(BlueprintType)
struct RETRIEVE_API FRetrieveInventorySaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TArray<FRetrieveItemStack> WeaponItems;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TArray<FRetrieveItemStack> ConsumableItems;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TArray<FRetrieveItemStack> MaterialItems;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TArray<FRetrieveItemStack> ArmorItems;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	FName EquippedWeaponId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TArray<FRetrieveEquippedArmorEntry> EquippedArmorSlots;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TMap<int32, FName> ConsumableSlotItemIds;
};

USTRUCT(BlueprintType)
struct RETRIEVE_API FWeaponSkillPreview
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill", meta = (Categories = "Ability"))
	FGameplayTag AbilityTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill", meta = (MultiLine = true))
	FText ShortDescription;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	TSoftObjectPtr<UTexture2D> Icon;
};

UENUM(BlueprintType)
enum class ERetrieveWeaponMeshType : uint8
{
	StaticMesh,
	SkeletalMesh
};

UENUM(BlueprintType)
enum class ERetrieveWeaponAttachTarget : uint8
{
	CharacterMeshSocket,
	OwnerRoot,
	OwnerComponentName,
	OwnerComponentTag
};

USTRUCT(BlueprintType)
struct RETRIEVE_API FWeaponComboStep
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Combo", meta = (ClampMin = "0.0"))
	float DamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Combo")
	FName SectionName = NAME_None;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Combo")
	ERetrieveHitReactType HitReactType = ERetrieveHitReactType::Flinch;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Combo")
	FGameplayTag ChargeBonusEventTag;

	/** 넉백 수평 강도(0이면 없음). 콤보 스텝별로 다르게 지정 가능. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Combo|Knockback", meta = (ClampMin = "0.0"))
	float KnockbackStrength = 0.f;

	/** 넉백 상향(Z) 강도. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Combo|Knockback", meta = (ClampMin = "0.0"))
	float KnockbackUpwardStrength = 0.f;
};

USTRUCT(BlueprintType)
struct RETRIEVE_API FAttackComboVariant
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Tags")
	FGameplayTag ElementTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Montage")
	TSoftObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Combo", meta = (TitleProperty = "SectionName"))
	TArray<FWeaponComboStep> ComboSteps;
};

USTRUCT(BlueprintType)
struct RETRIEVE_API FWeaponSprintAttack
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Tags")
	FGameplayTag ElementTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Montage")
	TSoftObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Combo", meta = (ClampMin = "0.0"))
	float DamageMultiplier = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Combo")
	FName SectionName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Combo")
	ERetrieveHitReactType HitReactType = ERetrieveHitReactType::Stagger;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Combo")
	FGameplayTag ChargeBonusEventTag;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Sprint", meta = (ClampMin = "0.0"))
	float RequiredSprintDuration = 0.5f;

	/** 넉백 수평 강도(0이면 없음). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Knockback", meta = (ClampMin = "0.0"))
	float KnockbackStrength = 0.f;

	/** 넉백 상향(Z) 강도. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Knockback", meta = (ClampMin = "0.0"))
	float KnockbackUpwardStrength = 0.f;
};

// JumpAttack 높이 구간, 발동 시점 지면 높이가 MinHeight 이상이면 후보가 되고 후보 중 MinHeight가 가장 큰 구간이 선택됨
USTRUCT(BlueprintType)
struct RETRIEVE_API FJumpAttackHeightTier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JumpAttack|Tier", meta = (ClampMin = "0.0"))
	float MinHeight = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JumpAttack|Tier", meta = (ClampMin = "0.0"))
	float DamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JumpAttack|Tier")
	ERetrieveHitReactType HitReactType = ERetrieveHitReactType::Flinch;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JumpAttack|Tier", meta = (ClampMin = "0.0"))
	float AoeRadiusOverride = 0.f;
};

USTRUCT(BlueprintType)
struct RETRIEVE_API FWeaponJumpAttack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Tags")
	FGameplayTag ElementTag;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Montage")
	TSoftObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Combo", meta = (ClampMin = "0.0"))
	float DamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Combo")
	FName SectionName = NAME_None;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Combo")
	FName LandingSectionName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Combo")
	ERetrieveHitReactType HitReactType = ERetrieveHitReactType::Flinch;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Combo")
	FGameplayTag ChargeBonusEventTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Slam", meta = (ClampMin = "0.0"))
	float LandingAoeRadius = 250.f;

	// 착지 AoE에 방사형 넉백 적용 여부. 끄면 기존 동작(넉백 없음) 유지.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Slam")
	bool bUseLandingKnockback = false;

	// 착지 AoE 방사형 넉백 파라미터. bUseLandingKnockback일 때만 적용.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Slam", meta = (EditCondition = "bUseLandingKnockback"))
	FRetrieveKnockbackParams LandingKnockback;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Slam", meta = (ClampMin = "0.0"))
	float DiveGravityScale = 0.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Slam")
	TArray<FJumpAttackHeightTier> HeightTiers;
};


// 패리 성공 후 카운터 공격 데이터, 적 타입(Normal, Boss)에 따라 다른 Groggy GE 적용
USTRUCT(BlueprintType)
struct RETRIEVE_API FParryCounterData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ParryCounter|Tags")
	FGameplayTag ElementTag;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ParryCounter|Motion")
	TSoftObjectPtr<UAnimMontage> CounterMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ParryCounter|Attack", meta = (ClampMin = "0.0"))
	float DamageMultiplier = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ParryCounter|Attack")
	ERetrieveHitReactType HitReactType = ERetrieveHitReactType::Stagger;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ParryCounter|Groggy")
	TSubclassOf<UGameplayEffect> NormalGroggyEffect;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ParryCounter|Groggy")
	TSubclassOf<UGameplayEffect> BossGroggyEffect;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ParryCounter|Groggy", meta = (ClampMin = "0.0"))
	float GroggyDuration = 3.f;

	/** 넉백 수평 강도(0이면 없음). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ParryCounter|Knockback", meta = (ClampMin = "0.0"))
	float KnockbackStrength = 0.f;

	/** 넉백 상향(Z) 강도. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ParryCounter|Knockback", meta = (ClampMin = "0.0"))
	float KnockbackUpwardStrength = 0.f;
};

// 스태프(배틀메이지) 강공격(왼손 투사체) 데이터
USTRUCT(BlueprintType)
struct RETRIEVE_API FWeaponStaffAttack
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staff|Projectile")
	TSubclassOf<AStaffProjectile> ProjectileClass;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staff|Projectile", meta = (ClampMin = "0.0"))
	float ProjectileSpeed = 1800.f;

	// 발사 시점(초). 비면 즉시 1발, 여러 개면 연사(몽타주 길이 내)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staff|Projectile")
	TArray<float> FireDelays;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staff|Projectile", meta = (ClampMin = "0.0"))
	float DamageMultiplier = 1.2f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staff|Projectile")
	ERetrieveHitReactType HitReactType = ERetrieveHitReactType::Stagger;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staff|Projectile")
	FName SpawnSocketName = NAME_None;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staff|Projectile")
	FVector SpawnOffset = FVector(60.f, -35.f, 50.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staff|Projectile")
	FGameplayTag ChargeBonusEventTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staff|Element")
	TMap<FGameplayTag, TSubclassOf<UGameplayEffect>> ElementStatusEffects;
};

USTRUCT(BlueprintType)
struct RETRIEVE_API FRetrieveWeaponAttachmentData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visual")
	FName PartName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visual")
	ERetrieveWeaponMeshType MeshType = ERetrieveWeaponMeshType::StaticMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visual")
	TSoftObjectPtr<UStaticMesh> StaticMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visual")
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visual")
	ERetrieveWeaponAttachTarget AttachTarget = ERetrieveWeaponAttachTarget::CharacterMeshSocket;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visual")
	FName AttachComponentName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visual")
	FName AttachComponentTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visual")
	FName AttachSocketName = TEXT("Weapon_R");

	// 납검(Sheathed) 시 부착할 캐릭터 소켓(등/허리). 비우면 SetWeaponDrawn(false)가 이 파트를 옮기지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visual")
	FName SheathedSocketName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visual")
	FTransform RelativeTransform = FTransform::Identity;
};

// 무기 데이터. 보유 상태는 InventoryComponent, 실제 장착 반영은 WeaponComponent에서 처리
USTRUCT(BlueprintType)
struct RETRIEVE_API FRetrieveWeaponDataRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (Categories = "Item"))
	FGameplayTag ItemTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (Categories = "Weapon.Type"))
	FGameplayTag WeaponTypeTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (Categories = "Weapon.Grade"))
	FGameplayTag WeaponGradeTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (Categories = "Weapon.Affinity"))
	FGameplayTag WeaponAffinityTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float AttackPower = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float ElementChargeMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visual")
	TArray<FRetrieveWeaponAttachmentData> Attachments;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat")
	TSoftObjectPtr<UDataTable> WeaponAttackTable;
	
	// 버스트 VFX 등 단일 지점 기준으로 쓰는 소켓
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat")
	FName TraceSocketName = TEXT("Weapon_R");
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat")
	FName TraceStartSocketName = TEXT("Weapon_Start");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat")
	FName TraceEndSocketName = TEXT("Weapon_End");
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat", meta = (ClampMin = "2"))
	int32 TraceSegmentCount = 4;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat", meta = (ClampMin = "0.0"))
	float TraceRadius = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat")
	TSoftObjectPtr<UAttackComboDefinition> AttackComboDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat")
	FWeaponSprintAttack SprintAttack;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat")
	FWeaponJumpAttack JumpAttack;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat")
	FParryCounterData ParryCounter;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat")
	TSoftObjectPtr<UAnimMontage> ParrySuccessMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat")
	FWeaponStaffAttack StaffAttack;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat", meta = (AllowedClasses = "/Script/Retrieve.RetrieveAbilitySet"))
	FSoftObjectPath WeaponAbilitySet;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat", meta = (Categories = "Ability"))
	TArray<FGameplayTag> GrantedAbilityTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Animation")
	TSoftClassPtr<UAnimInstance> UpperBodyAnimLayer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|UI")
	TArray<FWeaponSkillPreview> SkillPreviews;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|UI", meta = (MultiLine = true))
	FText ShortDescription;
};

// 방어구 데이터. 복제는 RowName을 사용하고, 외형은 VisualData를 로컬에서 재구성한다.
USTRUCT(BlueprintType)
struct RETRIEVE_API FRetrieveArmorDataRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Armor")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Armor")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Armor", meta = (Categories = "Item"))
	FGameplayTag ItemTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Armor", meta = (Categories = "Equipment.Slot"))
	FGameplayTag EquipmentSlotTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Armor")
	float Defense = 0.0f;

	/** 이 방어구가 장착하는 파츠들 (슬롯태그 → 메시). VisualMesh 아래에 spawn되어 LeaderPose로 따라간다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Armor|Visual", meta = (TitleProperty = PartSlotTag))
	TArray<FRetrieveArmorVisualPart> VisualParts;

	/** 이 방어구가 추가로 가릴 기본 바디 PartSlot. VisualParts가 직접 채우는 슬롯은 자동으로 가려지므로
	 *  여기에는 다른 슬롯만 적는다. (투구 → Hair / Attachment.Face) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Armor|Visual", meta = (Categories = "Cosmetic.Part"))
	FGameplayTagContainer SuppressedDefaultPartSlots;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Armor|Gameplay", meta = (AllowedClasses = "/Script/Retrieve.RetrieveAbilitySet"))
	FSoftObjectPath ArmorAbilitySet;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Armor|UI", meta = (MultiLine = true))
	FText ShortDescription;
};

// 소모 아이템 데이터. 실제 회복/버프 적용은 UseItem Ability에서 처리
USTRUCT(BlueprintType)
struct RETRIEVE_API FRetrieveConsumableItemRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consumable")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consumable")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consumable", meta = (Categories = "Item.Consumable"))
	FGameplayTag ItemTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consumable", meta = (Categories = "Element"))
	FGameplayTag ElementTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consumable")
	float HealAmount = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consumable")
	float ElementBuffMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consumable")
	float BuffDuration = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consumable|UI", meta = (Categories = "UI.Buff"))
	FGameplayTag BuffUITag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consumable", meta = (ClampMin = "1"))
	int32 MaxStack = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consumable|GAS")
	TSubclassOf<UGameplayEffect> HealEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consumable|GAS")
	TSubclassOf<UGameplayEffect> ElementBuffEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consumable|Animation")
	TSoftObjectPtr<UAnimMontage> UseMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consumable|Rules")
	FGameplayTagContainer BlockedStateTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consumable|UI", meta = (MultiLine = true))
	FText ShortDescription;
};

USTRUCT(BlueprintType)
struct RETRIEVE_API FRetrieveMaterialItemRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material", meta = (Categories = "Item.Material"))
	FGameplayTag ItemTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material", meta = (Categories = "Element"))
	FGameplayTag ElementTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material", meta = (ClampMin = "1"))
	int32 MaxStack = 99;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material|UI", meta = (MultiLine = true))
	FText ShortDescription;
};

// 화톳불 저장 시 플레이어 복원 기준 스냅샷
USTRUCT(BlueprintType)
struct RETRIEVE_API FRetrieveLoadSnapshotData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save")
	FName BonfireId = NAME_None;

	// World Partition 구조 — 단일 퍼시스턴트 레벨이므로 확장 지점으로만 유지
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save")
	FName LevelName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save")
	FTransform PlayerTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save")
	float SavedHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save")
	FName EquippedWeaponId = NAME_None;
};

// 인벤토리 / 진행 상태 저장 데이터
USTRUCT(BlueprintType)
struct RETRIEVE_API FRetrieveInventoryProgressSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save")
	FRetrieveInventorySaveData Inventory;

	// 퀘스트 / 대사 분기 구현 시 연결 — 현재는 확장 지점만 유지
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save")
	TMap<FName, FGameplayTag> ChoiceHistory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save")
	FGameplayTagContainer ProgressTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save")
	FGameplayTag FinalEndingChoiceTag;
};

// 제작 레시피 데이터 테이블 Row. RowName == RecipeId
USTRUCT(BlueprintType)
struct RETRIEVE_API FRetrieveCraftRecipeRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Craft")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Craft")
	TArray<FRetrieveItemStack> RequiredMaterials;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Craft")
	FRetrieveItemStack OutputItem;

	// 해당 태그를 보유한 경우에만 레시피 표시
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Craft")
	FGameplayTagContainer RequiredProgressTags;
};

// UI 아이콘 조회용 테이블. RowName은 ItemId와 동일하게 사용
USTRUCT(BlueprintType)
struct RETRIEVE_API FRetrieveItemIconRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Icon")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Icon")
	TSoftObjectPtr<UTexture2D> IconTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Icon")
	TSoftObjectPtr<UTexture2D> ElementIconTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Icon", meta = (Categories = "Item"))
	FGameplayTag ItemCategoryTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Icon")
	FGameplayTag ElementOrAffinityTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Icon")
	FLinearColor AccentColor = FLinearColor::White;
};

USTRUCT(BlueprintType)
struct RETRIEVE_API FQuestStep : public FTableRowBase
{
	GENERATED_BODY()
	
	/** 이 Step의 표준 ID. 행 이름이 아닌 태그로 조회함(Quest.Step.*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FGameplayTag StepTag;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest", meta = (MultiLine = true))
	FText TrackerText;
	
	/** 이 Step이 완료되기 전에 이미 완료되어 있어야 하는 태그 목록. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	TArray<FGameplayTag> Prerequisites;
	
	/** 선행 조건이 충족되는 즉시 자동 완료됨: SealUnlocked만 true */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	bool bAutoCompleteWhenPrereqsMet = false;
	
	/** GameState를 ERetrieveSessionState::Result로 전환함: GameComplete만 true */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	bool bAdvancesSessionToResult = false;
	
	/** <Element>SigilActivated 행만 채우기: Channel.Quest.GuardianDefeated 페이로드에 포함됨. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FGameplayTag UnlockElementTag;
};

USTRUCT(BlueprintType)
struct RETRIEVE_API FDialogueRow : public FTableRowBase
{
	GENERATED_BODY()
	
	// ---- Identity + gating (모든 행 공통)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FGameplayTag SpeakerTag; // Speaker.Lumen, Speaker.NPC.<id>
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FGameplayTag TopicId;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	ETopicKind Kind = ETopicKind::Story;
	
	/* Topic 버튼에 표시될 텍스트 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FText Label;
	
	/** Topic 선택 이후 표시되는 대사 라인들 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	TArray<FText> Lines;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FGameplayTag RequiresStep;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FGameplayTag BlockedByStep;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	int32 Order = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	int32 Priority = 0;
	
	// ---- Story 행 전용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Story")
	TArray<FName> FollowUpRows; // 이 대사 이후 Topic이 표시될 행 이름; 비어있으면 끝
	
	// ---- Command 행 전용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Command")
	FGameplayTag CommandChannel; // e.g. Channel.Lumen.Command.ToggleWait
	
	// ── Sigil 행 전용 — ApplySigilTopic에서 소비 ────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Sigil")
	FGameplayTag SigilStepTag; // Quest.Step.SigilCompleted | <E>SigilActivated

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Sigil")
	FGameplayTag VfxCue;   
	
	// Note: UnlockGE / UnlockElementTag 없음. Sigil과 매핑되는 원소는
	// 해당 DT_QuestStep 행 (FQuestStep.UnlockElementTag)에 있으며, CompleteStep이 읽는다.
	// TODO: Channel.Quest.GuardianDefeated{element}이 발행되면 이후 해당 원소 모드 강화
};

// ---- 버프/디버프 UI DataTable ------------------------------------------------

/**
 * DT_BuffUIDefinitions 행.
 * Row Name = UI 태그 문자열 그대로 (예: "UI.Buff.Debuff.Slow")
 * GE Asset Tag에 이 태그가 달려 있으면 URetrieveBuffUIBroadcastComponent가
 * 자동 감지해 버프 바(Channel_UI_Buff_Apply)에 표시한다.
 */
USTRUCT(BlueprintType)
struct RETRIEVE_API FRetrieveBuffUIRow : public FTableRowBase
{
	GENERATED_BODY()

	/** GE.AssetTags에 포함돼야 할 태그. Row Name과 일치시킨다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BuffUI")
	FGameplayTag BuffUITag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BuffUI", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BuffUI", meta = (MultiLine = true))
	FText EffectSummary;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BuffUI|GAS")
	TSubclassOf<UGameplayEffect> LinkedGameplayEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BuffUI")
	FText DisplayName;

	/** 버프 바에 표시할 아이콘. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BuffUI")
	TSoftObjectPtr<UTexture2D> Icon;

	/** 아이콘 틴트. 버프는 하늘색, 디버프는 보라/빨강 계열 권장. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BuffUI")
	FLinearColor TintColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BuffUI")
	bool bIsDebuff = false;

	/**
	 * true이면 같은 BuffId로 Apply가 여러 번 들어올 때 스택을 쌓아 ×N으로 표시한다.
	 * 흡수 원소 버프처럼 중첩될 수 있는 경우에만 활성화.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BuffUI")
	bool bIsStackable = false;

	/**
	 * 스택 최대치. 0이면 무제한.
	 * bIsStackable이 true일 때만 유효하며, 이 값을 초과하는 Apply는 지속시간만 리셋하고 스택을 늘리지 않는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BuffUI", meta = (ClampMin = 0, EditCondition = "bIsStackable"))
	int32 MaxStack = 0;

	/**
	 * 0이면 GE의 실제 지속시간을 자동으로 읽는다.
	 * 0 초과이면 이 값을 사용한다 (Instant GE 또는 BroadcastBuffManual 비-GAS 표시용).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BuffUI", meta = (ClampMin = 0.0))
	float DurationOverride = 0.f;
};
