#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"

#include "GameplayTagContainer.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "RetrieveDataTableTypes.generated.h"

class UStateTree;
class UCameraShakeBase;
class UAnimInstance;
class UAnimMontage;
class UGameplayEffect;
class URetrieveAbilitySet;
class USkeletalMesh;
class UStaticMesh;
class UTexture2D;
class UNiagaraSystem;
class USoundBase;

USTRUCT(BlueprintType)
struct RETRIEVE_API FCharacterStats : public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Stats")
	float MaxHealth = 100.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Stats")
	float AttackPower = 0.0f;
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
	
	// 충돌체 관련
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Hitbox")
	FName HitboxBoneName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Pattern|Hitbox", meta=(ClampMin="0.0"))
	float HitboxRadius = 30.f;

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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit", meta = (Categories = "GameplayEvent"))
	FGameplayTagContainer HitEventTags;
	// 대미지 숫자 플로터 크기 배수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Number", meta = (ClampMin = "0.5", UIMin = "0.5", UIMax = "3.0"))
	float DamageNumberScale = 1.0f;
	// 대미지 숫자 색상(강도별 시각 차별)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Number")
	FLinearColor DamageNumberColor = FLinearColor::White;
};

UENUM(BlueprintType)
enum class EBurstAttackType : uint8
{
	Cleave           UMETA(DisplayName = "단일 강타"),
	GroundEruption   UMETA(DisplayName = "지면 분출"),
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

	/** 이 타격에서 재생할 적중 VFX. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit")
	TSoftObjectPtr<UNiagaraSystem> HitVFX;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|FX")
	TSoftObjectPtr<USoundBase> HitSound;

	/** 이 타격이 적중한 대상에 순차 부여할 상태 GE들. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit|Status")
	TArray<TSubclassOf<UGameplayEffect>> StatusEffects;
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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Element")
	TMap<FGameplayTag, int32> ElementPattern;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Motion")
	UAnimMontage* AttackMontage;
	// ---- Attack --------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Attack")
	EBurstAttackType AttackType = EBurstAttackType::Cleave;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Attack|Projectile", meta = (EditCondition = "AttackType == EBurstAttackType::Projectile"))
	TSubclassOf<AActor> ProjectileClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Attack|Dash", meta = (EditCondition = "AttackType == EBurstAttackType::Dash", ClampMin = "0.0"))
	float DashDistance = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Attack|AoE", meta = (EditCondition = "AttackType == EBurstAttackType::AreaOfEffect", ClampMin = "0.0"))
	float AoeRadius = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Attack|World", meta = (EditCondition = "AttackType == EBurstAttackType::GroundEruption", ClampMin = "0.0"))
	float WorldSpawnDistance = 300.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Attack|World", meta = (EditCondition = "AttackType == EBurstAttackType::GroundEruption"))
	TSubclassOf<AActor> WorldSpawnActorClass;
	// ---- Damage --------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Damage")
	TSubclassOf<UGameplayEffect> DamageEffect;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Damage")
	TArray<FBurstHitInstance> HitSequence;
	// ---- FX --------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|FX")
	TSoftObjectPtr<USoundBase> CastSound;
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
	FName EquippedWeaponId = NAME_None;

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
