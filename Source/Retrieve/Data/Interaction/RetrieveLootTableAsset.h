#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Data/Interaction/RetrievePickupGroupResultAsset.h"
#include "RetrieveLootTableAsset.generated.h"

/**
 * LootTable의 단일 드롭 엔트리 — 확률 + 수량 범위 + 가중치 정의.
 */
USTRUCT(BlueprintType)
struct RETRIEVE_API FRetrieveLootEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Loot")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Loot",
		meta = (Categories = "Item"))
	FGameplayTag ItemCategoryTag;

	/** 드롭 시 수량의 최솟값 (포함) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Loot",
		meta = (ClampMin = "1"))
	int32 MinQuantity = 1;

	/** 드롭 시 수량의 최댓값 (포함) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Loot",
		meta = (ClampMin = "1"))
	int32 MaxQuantity = 1;

	/**
	 * 가중치 — 다른 엔트리들과 비교한 상대 확률.
	 * 예: A=60, B=25, C=15 → A가 60% 확률로 선택됨 (Independent 모드면 무시)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Loot",
		meta = (ClampMin = "0.0"))
	float Weight = 1.0f;

	/**
	 * Independent 모드 전용 — 이 엔트리가 드롭될 개별 확률 (0~1).
	 * 가중치 풀 모드(bRollIndependent=false)에서는 무시됨.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Loot",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DropChance = 1.0f;

	/** true면 항상 드롭 (확률 무시). 보장 드롭 (퀘스트 핵심 아이템 등) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Loot")
	bool bGuaranteed = false;
};

/**
 * 드롭 정의 묶음 DataAsset.
 *
 * 두 가지 굴림 모드:
 *   - bRollIndependent = false (기본, 가중치 풀 모드)
 *       매 굴림마다 가중치에 비례한 추첨으로 1개 엔트리 선택.
 *       MinRolls~MaxRolls 횟수만큼 굴림 → 결과 아이템 N개.
 *
 *   - bRollIndependent = true (독립 굴림 모드)
 *       각 엔트리마다 DropChance 확률로 독립적으로 드롭 결정.
 *       MinRolls/MaxRolls는 무시.
 *
 * bGuaranteed 엔트리는 모드와 무관하게 항상 결과에 포함됨.
 *
 * 사용 시나리오:
 *   - 일반 몬스터 드롭: 가중치 풀 (Gold 60 / Herb 25 / Iron 15), Min=1 Max=2
 *   - 보스 드롭: Independent (각 보상이 따로 확률 체크 + Guaranteed로 핵심 아이템 보장)
 *   - 광맥 채집: 가중치 풀, Min=1 Max=3, 매 채집마다 다른 양
 */
UCLASS(BlueprintType)
class RETRIEVE_API URetrieveLootTableAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Editor에서 식별용 표시 이름 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Loot")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Loot")
	TArray<FRetrieveLootEntry> Entries;

	/**
	 * 가중치 풀 모드의 최소 굴림 횟수 (Independent 모드에서는 무시).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Loot|Rolls",
		meta = (ClampMin = "0"))
	int32 MinRolls = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Loot|Rolls",
		meta = (ClampMin = "1"))
	int32 MaxRolls = 1;

	/**
	 * true면 각 엔트리를 DropChance로 독립 굴림. false면 가중치 풀에서 N번 추첨.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Loot|Rolls")
	bool bRollIndependent = false;

	/**
	 * 굴림 실행 — 결과 아이템 목록(수량 포함) 반환.
	 * @param Stream  랜덤 스트림 (deterministic 굴림이 필요하면 외부에서 시드 제어)
	 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Loot")
	TArray<FRetrievePickupEntry> RollLoot(FRandomStream& Stream) const;
};
