#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Shop/RetrieveShopTypes.h"
#include "RetrieveSaveGame.generated.h"

USTRUCT(BlueprintType)
struct FRetrieveRescueEncounterSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save|Rescue")
	bool bEnemiesDefeated = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save|Rescue")
	bool bRewardClaimed = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save|Rescue")
	bool bMerchantUnlocked = false;
};

USTRUCT(BlueprintType)
struct FRetrieveLostCargoSaveData
{
	GENERATED_BODY()

	/** ERetrieveLostCargoState serialized as a byte to keep SaveGame independent from the encounter actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save|Lost Cargo")
	uint8 State = 0;
};

/** 제너릭 퀘스트 인카운터의 목표 1개 진행도(바이트 블롭). */
USTRUCT()
struct FRetrieveQuestObjectiveSaveData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<uint8> Bytes;
};

/** EncounterId별 제너릭 퀘스트 진행 상태(단계 + 목표별 진행도). */
USTRUCT(BlueprintType)
struct FRetrieveQuestSaveData
{
	GENERATED_BODY()

	/** ERetrieveQuestPhase를 바이트로 직렬화. */
	UPROPERTY()
	uint8 Phase = 0;

	UPROPERTY()
	TArray<FRetrieveQuestObjectiveSaveData> Objectives;
};
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

	/** 잊혀진 영웅 장비 진화 충전량(0~임계치). 세트 착용 중 흡수/버스트 시 누적. 월드 공유 진행 상태. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save")
	int32 HeroEvolutionCharge = 0;

	/** 잊혀진 영웅 장비가 전설 영웅 장비로 진화 완료됐는지 여부. 월드 공유 진행 상태. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save")
	bool bHeroEquipmentEvolved = false;

	/** 전장의 안개 탐색 마스크(0=미탐색, 255=탐색). 정사각, 크기 = ExploredMaskResolution^2. */
	UPROPERTY()
	TArray<uint8> ExploredMask;

	/** 위 마스크의 한 변 해상도. 0이면 저장된 안개 데이터 없음. */
	UPROPERTY()
	int32 ExploredMaskResolution = 0;

	/** NPC 대화 랜덤 보상 지급 횟수 (SpeakerTag별, 월드 공유 진행 상태. NPC당 최대 횟수 제한용) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save")
	TMap<FGameplayTag, int32> DialogueRewardGrantCounts;

	/** 가위바위보 내기 3연승 보상 지급 횟수 (SpeakerTag별, 월드 공유 진행 상태) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save")
	TMap<FGameplayTag, int32> RpsRewardGrantCounts;

	/** EncounterId별 구출 이벤트 진행 상태. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save|Rescue")
	TMap<FName, FRetrieveRescueEncounterSaveData> RescueEncounters;

	/** EncounterId별 도난 짐 회수 이벤트 진행 상태. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save|Lost Cargo")
	TMap<FName, FRetrieveLostCargoSaveData> LostCargoEncounters;

	/** EncounterId별 제너릭 퀘스트 인카운터 진행 상태. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save|Quest")
	TMap<FName, FRetrieveQuestSaveData> QuestEncounters;

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

	/** 저장 당시 추적 중이던 퀘스트 표시 이름 (없으면 빈 문자열) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save|Meta")
	FString TrackedQuestName;

	/** 저장 당시 추적 퀘스트의 첫 미완료 목표 문구 (없으면 빈 문자열) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save|Meta")
	FString TrackedQuestObjective;

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

	/** 상점별 소진 재고. 키=상점 정의 에셋 이름(ShopId), 값=RowName별 남은 수량.
	 *  유한 재고 상점에서 구매할 때마다 차감·저장된다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save")
	TMap<FName, FRetrieveShopStockSave> ShopStock;

	// ── 퀘스트 진행 상태 (QuestBranchComponent 미러) ─────────────────────────────

	/** 완료된 퀘스트 스텝 목록 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save|Quest")
	TArray<FGameplayTag> CompletedQuestSteps;

	/** 현재 추적 중인 퀘스트 스텝 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save|Quest")
	FGameplayTag CurrentTrackerStep;

	/** 대화 분기 선택 기록 (ChoiceId → 선택된 태그) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Save|Quest")
	TMap<FName, FGameplayTag> QuestChoiceHistory;
};
