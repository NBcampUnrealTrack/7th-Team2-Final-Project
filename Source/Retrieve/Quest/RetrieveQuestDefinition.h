#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "RetrieveQuestDefinition.generated.h"

class USkeletalMesh;
class UMaterialInterface;
class UAnimInstance;
class UAnimMontage;

/** 퀘스트 진행 단계. (정의/인카운터 공용 — 순환참조 방지를 위해 여기에 둔다) */
UENUM(BlueprintType)
enum class ERetrieveQuestPhase : uint8
{
	/** 아직 수락 전. NPC가 의뢰 대사를 보여준다. */
	Offered,
	/** 진행 중. 목표들이 활성화되어 완료를 감시한다. */
	InProgress,
	/** 모든 목표 완료. NPC와 대화하면 보상 수령(턴인). */
	ReadyToTurnIn,
	/** 완료됨. */
	Completed
};

/** 목표 유형. 데이터 행에서 런타임 목표 UObject를 생성할 때 분기 키. */
UENUM(BlueprintType)
enum class EQuestObjectiveKind : uint8
{
	/** 스폰 그룹 몬스터 전멸. */
	ClearSpawnGroup,
	/** 월드에 스폰된 물건과 상호작용해 회수. */
	CollectWorldItem,
	/** 지정 위치(인카운터 기준) 도달. */
	ReachLocation,
	/** 특정 아이템 N개 획득(몬스터 드랍/수집). */
	AcquireItem
};

/** 보상 아이템 1종. */
USTRUCT(BlueprintType)
struct FRetrieveQuestItemReward
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Reward")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Reward", meta = (Categories = "Item"))
	FGameplayTag ItemCategoryTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Reward", meta = (ClampMin = "1"))
	int32 Quantity = 1;
};

/**
 * 데이터로 표현한 목표 1개. 모든 유형의 파라미터를 평면적으로 담으며,
 * 인카운터가 Kind에 따라 필요한 필드만 읽어 런타임 목표 UObject를 만든다.
 */
USTRUCT(BlueprintType)
struct FRetrieveQuestObjectiveDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|ObjectiveDef")
	EQuestObjectiveKind Kind = EQuestObjectiveKind::ClearSpawnGroup;

	/** 트래커/다이얼로그 표시 설명. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|ObjectiveDef", meta = (MultiLine = true))
	FText Description;

	/** 이 목표를 완료하는 순간 아이템 획득 토스트로 띄울 메시지. 비우면 토스트 없음(예: "탑을 청소했습니다"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|ObjectiveDef")
	FText CompletionToastMessage;

	// ── ClearSpawnGroup ────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|ObjectiveDef")
	FGameplayTag SpawnGroupId;

	// ── AcquireItem ────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|ObjectiveDef")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|ObjectiveDef", meta = (Categories = "Item"))
	FGameplayTag ItemCategoryTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|ObjectiveDef", meta = (ClampMin = "1"))
	int32 RequiredCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|ObjectiveDef")
	bool bConsumeOnTurnIn = true;

	// ── ReachLocation ──────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|ObjectiveDef", meta = (ClampMin = "50.0"))
	float AcceptanceRadius = 350.f;

	// ── CollectWorldItem / ReachLocation 대상 스폰 위치 ─────────────────
	/** 대상(회수 물건/목적지 마커)을 인카운터 기준 이 오프셋에 스폰. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|ObjectiveDef")
	FVector TargetSpawnOffset = FVector::ZeroVector;
};

/** 애니메이션 호환 스켈레탈 외형 1종(퀘스트 NPC 랜덤 외형 풀 항목). */
USTRUCT(BlueprintType)
struct FRetrieveQuestNPCAppearance
{
	GENERATED_BODY()

	/** 표시 이름(생성 툴 UI/디버그용). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Appearance")
	FText DisplayName;

	/** 스켈레탈 메시. NPC AnimBP와 호환되는 스켈레톤이어야 한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Appearance")
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	/** 슬롯 순서대로 적용할 오버라이드 머티리얼(선택). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Appearance")
	TArray<TSoftObjectPtr<UMaterialInterface>> OverrideMaterials;

	/** 이 메시에 강제할 AnimBP(선택). 비우면 NPC 클래스 기본 AnimBP 유지. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Appearance")
	TSoftClassPtr<UAnimInstance> AnimClass;
};

/**
 * 퀘스트 NPC 랜덤 외형 풀. 애니메이션이 정상 동작하는 스켈레탈 메시만 큐레이션한다.
 * 생성 툴이 여기서 인덱스를 뽑아 퀘스트 행에 baked(결정적)로 저장한다.
 */
UCLASS(BlueprintType)
class RETRIEVE_API URetrieveQuestNPCAppearancePool : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Appearance")
	TArray<FRetrieveQuestNPCAppearance> Appearances;

	/** 인덱스로 외형 항목을 반환(범위 밖이면 nullptr). */
	const FRetrieveQuestNPCAppearance* Get(int32 Index) const
	{
		return Appearances.IsValidIndex(Index) ? &Appearances[Index] : nullptr;
	}
};

/**
 * 퀘스트 1개의 데이터 정의(DataTable 행). RowName == QuestDefId.
 * 인카운터가 QuestDefId로 이 행을 읽어 목표·다이얼로그·보상·스폰을 런타임 구성한다.
 */
USTRUCT(BlueprintType)
struct RETRIEVE_API FRetrieveQuestDefRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Def")
	FText Title;

	/** true면 NPC와 대화해 수락해야 시작. false면 배치 즉시 진행(구출형). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Def")
	bool bRequiresOffer = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Def")
	TArray<FRetrieveQuestObjectiveDef> Objectives;

	// ── 다이얼로그 ─────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Def|Dialogue")
	TArray<FText> OfferDialogueLines;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Def|Dialogue")
	TArray<FText> InProgressDialogueLines;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Def|Dialogue")
	TArray<FText> ReadyToTurnInDialogueLines;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Def|Dialogue")
	TArray<FText> CompletedDialogueLines;

	// ── 보상 ───────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Def|Reward", meta = (ClampMin = "0"))
	int32 GoldReward = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Def|Reward")
	TArray<FRetrieveQuestItemReward> ItemRewards;

	// ── NPC 외형/배치 ──────────────────────────────────────────────────
	/** 대화창에 표시할 NPC 이름. 비우면 NPC 클래스 기본값 유지. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Def|NPC")
	FText NPCDisplayName;

	/** 외형 풀 인덱스(생성 시 결정). -1이면 외형 미적용(NPC 클래스 기본). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Def|NPC")
	int32 AppearanceIndex = -1;

	/** NPC 스폰 상대 위치. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Def|NPC")
	FVector NPCSpawnOffset = FVector::ZeroVector;

	/** NPC 스폰 상대 회전. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Def|NPC")
	FRotator NPCSpawnRotation = FRotator::ZeroRotator;

	/** true면 완료(Completed) 단계에서 NPC를 숨긴다(예: 구출된 뒤 사라지는 포로). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Def|NPC")
	bool bHideNPCOnComplete = false;

	/** NPC가 특정 단계 동안 루프 재생할 몽타주(예: 포로의 기도). 비우면 미사용. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Def|NPC")
	TSoftObjectPtr<UAnimMontage> NPCIdleMontage;

	/** 위 몽타주를 재생할 단계들. 비면 재생 안 함. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Def|NPC")
	TArray<ERetrieveQuestPhase> NPCIdleMontagePhases;

	// ── 완료 시 상인(상점) 해방 ─────────────────────────────────────────
	/** 설정 시 이 클래스를 스폰해 완료(Completed) 단계에만 노출한다(예: 해방된 상인 상점 NPC). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Def|Merchant")
	TSoftClassPtr<AActor> MerchantClass;

	/** 상인 스폰 상대 위치(모닥불 기준 또는 인카운터 기준). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Def|Merchant")
	FVector MerchantSpawnOffset = FVector::ZeroVector;

	/** true면 상인을 인카운터가 아니라 가장 가까운 모닥불에 스폰한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Def|Merchant")
	bool bMerchantAtNearestBonfire = true;
};
