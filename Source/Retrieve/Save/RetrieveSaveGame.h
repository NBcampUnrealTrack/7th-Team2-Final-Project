#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Shop/RetrieveShopTypes.h"
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

	/** 가디언 코어 흡수로 해방된 원소 태그 목록 (월드 공유 진행 상태) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save")
	FGameplayTagContainer UnlockedElements;

	/** 루멘 각인 완료 여부 (월드 공유 진행 상태) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save")
	bool bLumenEngraved = false;

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

	/**
	 * 저장 시점의 게임 화면 썸네일(PNG).
	 * UI가 포함되지 않는 별도 SceneCapture로 생성하므로 저장 메뉴는 화면에서 깜빡이지 않는다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Save|Meta")
	TArray<uint8> ScreenshotPng;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Save|Meta")
	int32 ScreenshotWidth = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Save|Meta")
	int32 ScreenshotHeight = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save")
	int32 SaveVersion = 2;

	/** 상점 판매 재구매 히스토리 (최대 20건 FIFO). 판매 시 기록, 재구매 시 제거 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save")
	TArray<FShopRepurchaseRecord> ShopRepurchaseHistory;
};
