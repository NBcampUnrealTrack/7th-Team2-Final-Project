#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Data/RetrieveDataTableTypes.h"
#include "RetrieveSaveGame.generated.h"

/**
 * 화톳불 기반 저장 데이터 컨테이너.
 *
 * LoadSnapshot       — 사망/로드 시 플레이어 복원 기준 (위치, HP, 장착 무기)
 * InventoryProgress  — 인벤토리 / 퀵슬롯 / 진행 태그
 * ActivatedBonfires  — World Partition 내 활성화된 화톳불 목록 (빠른 이동용)
 * SlotMeta           — UI 표시용 슬롯 정보 (슬롯 인덱스, 저장 시각, 화톳불 이름)
 */
UCLASS(BlueprintType)
class RETRIEVE_API URetrieveSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save")
	FRetrieveLoadSnapshotData LoadSnapshot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save")
	FRetrieveInventoryProgressSaveData InventoryProgress;

	/** 활성화된 화톳불 ID → 도착 Transform 매핑 (빠른 이동용) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save")
	TMap<FName, FTransform> ActivatedBonfireTransforms;

	// ── 슬롯 메타데이터 (UI 표시용) ─────────────────────────────────────────────

	/** 슬롯 인덱스 (0~MaxSaveSlots-1). WorldState 슬롯은 -1 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save|Meta")
	int32 SlotIndex = 0;

	/** 저장 시각 문자열. 예) "2026-06-01 10:35" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save|Meta")
	FString SaveTimestamp;

	/** 저장 당시 화톳불 표시 이름 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save|Meta")
	FString BonfireDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save")
	int32 SaveVersion = 1;
};
