#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/EngineTypes.h"
#include "Save/RetrieveSaveGame.h"
#include "RetrieveSaveSubsystem.generated.h"

class UInventoryComponent;
class APawn;
class UCharacterMovementComponent;
class UWorldPartitionStreamingSourceComponent;
class UUserWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRetrieveSaveCompleted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRetrieveLoadCompleted);

/** 빠른 이동 시작 — UI에서 LoadingCutscene 표시용 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFastTravelStarted, FName, BonfireId);
/** 빠른 이동 완료 — UI에서 LoadingCutscene 숨김용 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFastTravelCompleted);

/**
 * 화톳불 기반 저장 / 로드 / 빠른 이동 관리자.
 *
 * 슬롯 구조:
 *   RetrieveSave_WorldState — 화톳불 활성화 자동 저장 (월드 공유 상태)
 *   RetrieveSave_Slot0 ~ Slot(MaxSaveSlots-1) — 플레이어 수동 저장 슬롯
 *
 * 싱글플레이: PlayerController 기반 슬롯 구분 없이 인덱스 기반으로 관리.
 * 멀티플레이 확장: GetSlotNameForPlayer()에 UniqueNetId 접두어 추가.
 */
UCLASS(BlueprintType, Config = Game)
class RETRIEVE_API URetrieveSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// ── 슬롯 상수 / 유틸 ──────────────────────────────────────────────

	/** 플레이어 수동 저장 슬롯 최대 개수 */
	static constexpr int32 MaxSaveSlots = 5;

	/** 슬롯 인덱스 → 슬롯 파일명 */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Save")
	static FString GetSlotName(int32 SlotIndex);

	// ── 멀티슬롯 저장 / 로드 ──────────────────────────────────────────

	/**
	 * 지정 슬롯에 저장.
	 * @param PlayerController  저장 기준 플레이어 컨트롤러
	 * @param BonfireId         저장 지점 화톳불 ID
	 * @param SlotIndex         저장할 슬롯 인덱스 (0 ~ MaxSaveSlots-1)
	 * @param BonfireDisplayName  UI 표시용 화톳불 이름
	 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Save")
	bool SaveToSlot(APlayerController* PlayerController, FName BonfireId,
	                int32 SlotIndex, const FString& BonfireDisplayName);

	/**
	 * 지정 슬롯에서 로드 후 플레이어에 적용.
	 * @param PlayerController  복원 대상 플레이어 컨트롤러
	 * @param SlotIndex         로드할 슬롯 인덱스
	 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Save")
	bool LoadFromSlot(APlayerController* PlayerController, int32 SlotIndex);

	/** 지정 슬롯의 저장 데이터 반환 (UI 표시용). 없으면 nullptr */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Save")
	URetrieveSaveGame* GetSaveGameForSlot(int32 SlotIndex) const;

	/** 저장 파일이 존재하는 슬롯 인덱스 목록 반환 */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Save")
	TArray<int32> GetExistingSaveSlotIndices() const;

	/**
	 * 다음 빈 슬롯 인덱스 반환.
	 * 모든 슬롯이 찼으면 0(가장 오래된 슬롯 덮어쓰기 기준)을 반환.
	 */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Save")
	int32 GetNextFreeSlotIndex() const;

	/** 저장 시각이 가장 최근인 슬롯 인덱스 반환(메인메뉴 "이어하기"용). 세이브가 없으면 -1. */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Save")
	int32 GetMostRecentSaveSlotIndex() const;

	/** 현재 활성 슬롯 인덱스 (-1 = 슬롯 미선택) */
	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Save")
	int32 ActiveSlotIndex = -1;

	// ── 레거시 저장 / 로드 (하위 호환) ───────────────────────────────

	/** @deprecated SaveToSlot 사용 권장. Slot0에 저장. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Save")
	bool SaveAtBonfire(APlayerController* PlayerController, FName BonfireId);

	/** @deprecated LoadFromSlot 사용 권장. Slot0에서 로드. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Save")
	bool LoadGame(APlayerController* PlayerController);

	/** 싱글플레이용 간편 버전. WorldState 파일 존재 여부 반환 */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Save")
	bool HasSaveGame() const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Save")
	URetrieveSaveGame* GetCurrentSaveGame() const { return CurrentSaveGame; }

	/** CurrentSaveGame을 WorldState 슬롯에 즉시 기록 (PlayerController 없이). 상점 판매 등 인게임 이벤트용 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Save")
	void FlushWorldState();

	UFUNCTION(BlueprintPure, Category = "Retrieve|Save")
	bool HasSaveGameForPlayer(APlayerController* PlayerController) const;

	// ── 화톳불 활성화 ─────────────────────────────────────────────────

	/**
	 * 화톳불 활성화 등록.
	 * WorldState 슬롯에 즉시 파일 저장.
	 * @param BonfireDisplayName  UI / 세이브 슬롯 표시용 화톳불 이름
	 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Save")
	void MarkBonfireActivated(FName BonfireId, FTransform ArrivalTransform,
	                          const FString& BonfireDisplayName = TEXT(""));

	UFUNCTION(BlueprintPure, Category = "Retrieve|Save")
	bool IsBonfireActivated(FName BonfireId) const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Save")
	bool GetBonfireTransform(FName BonfireId, FTransform& OutTransform) const;

	/**
	 * 에디터에서 활성으로 배치된 모닥불을 런타임에 등록한다(디스크 저장 X).
	 * MarkBonfireActivated와 달리 세이브 파일을 건드리지 않으므로 실제 진행 상태를 오염시키지 않는다.
	 * 이미 활성 기록이 있으면(세이브 또는 이전 시드) 덮어쓰지 않는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Save")
	void RegisterDefaultBonfire(FName BonfireId, const FTransform& ArrivalTransform);

	// ── 원소 해방 (월드 공유 진행 상태) ───────────────────────────────

	/**
	 * 원소 해방 등록. WorldState 슬롯에 즉시 파일 저장.
	 * 중복 태그는 무시.
	 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Save")
	void MarkElementUnlocked(FGameplayTag ElementTag);

	UFUNCTION(BlueprintPure, Category = "Retrieve|Save")
	bool IsElementUnlocked(FGameplayTag ElementTag) const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Save")
	FGameplayTagContainer GetUnlockedElements() const;

	/** 루멘 각인 완료 여부 기록. WorldState 슬롯에 즉시 파일 저장. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Save")
	void SetLumenEngraved(bool bEngraved);

	UFUNCTION(BlueprintPure, Category = "Retrieve|Save")
	bool IsLumenEngraved() const;

	// ── 빠른 이동 (World Partition) ───────────────────────────────────

	UFUNCTION(BlueprintCallable, Category = "Retrieve|FastTravel")
	void FastTravelToBonfire(FName BonfireId, APlayerController* PC);

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|FastTravel")
	float FastTravelStreamDelay = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|FastTravel")
	FVector FastTravelArrivalOffset = FVector(0.f, 0.f, 50.f);

	/** 목적지 스트리밍 완료를 폴링하는 간격(초). */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|FastTravel")
	float FastTravelStreamPollInterval = 0.1f;

	/** 스트리밍 소스 등록 직후 오판을 막기 위한 최소 대기 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|FastTravel")
	float FastTravelStreamMinWait = 0.3f;

	/** 스트리밍이 끝나지 않아도 강제로 진행하는 최대 대기 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|FastTravel")
	float FastTravelStreamTimeout = 10.0f;

	/** 텔레포트 후 로딩 오버레이를 유지해 카메라 컷/스냅·잔여 팝인을 가리는 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|FastTravel")
	float FastTravelOverlayHoldAfterArrival = 0.6f;

	/** 도착 후 목적지 기준으로 텍스처/셀/Nanite/VT 요청이 충분히 쌓이도록 주는 최소 유지 시간(초). */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Retrieve|FastTravel")
	float FastTravelSettleMinRenderTime = 0.8f;

	/** 완료 상태가 이만큼 "연속" 유지돼야 진짜 완료로 인정(첫 틱 오판 방지)(초). */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Retrieve|FastTravel")
	float FastTravelSettleStableDuration = 0.5f;

	/** 텍스처/메시 LOD까지 안정되길 기다리는 최대 시간(초). 초과 시 강제로 오버레이 해제. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Retrieve|FastTravel")
	float FastTravelSettleTimeout = 15.0f;

	/** 불러오기 시 SaveSubsystem이 직접 띄우는 로딩화면 위젯(WBP_LoadingCutscene). 재빌드 없이 ini로 변경 가능. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Retrieve|FastTravel")
	TSoftClassPtr<UUserWidget> LoadingScreenWidgetClass;

	// ── 델리게이트 ────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Save")
	FOnRetrieveSaveCompleted OnSaveCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Save")
	FOnRetrieveLoadCompleted OnLoadCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|FastTravel")
	FOnFastTravelStarted OnFastTravelStarted;

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|FastTravel")
	FOnFastTravelCompleted OnFastTravelCompleted;

private:
	void FinishFastTravel();

	/** 빠른 이동/불러오기 공용: 도착 위치로 텔레포트하고 스트리밍이 안정될 때까지 대기한다. */
	void BeginStreamedTeleport(APlayerController* PC, const FTransform& ArrivalTransform, FName BonfireIdForRecompute);

	/** 불러오기 등에서 SaveSubsystem이 직접 로딩화면을 띄운다(빠른 이동은 WorldMapWidget이 띄움). */
	void ShowLoadingScreen(APlayerController* PC);
	void HideLoadingScreen();

	/** 목적지 스트리밍 완료를 폴링한다(반복 타이머 콜백). */
	void PollFastTravelStreaming();

	/** 스트리밍 완료/타임아웃 후 지면 스냅 + 텔레포트 + 카메라 컷을 수행한다. */
	void PerformFastTravelArrival();

	/** 도착 후 레벨+텍스처/메시 LOD 스트리밍이 안정될 때까지 기다린 뒤 오버레이를 해제한다. */
	void PollFastTravelSettle();

	/** 빠른 이동용 임시 스트리밍 소스 액터를 파괴/해제한다. */
	void CleanupFastTravelStreamingSource();

	static UInventoryComponent* FindInventoryComponent(AActor* Actor);
	static bool IsValidSaveSlot(int32 SlotIndex);

	/** 내부 공통 저장 로직. SlotSave 오브젝트를 채우고 지정 슬롯명에 씀 */
	bool WriteSaveToSlot(URetrieveSaveGame* SlotSave, const FString& SlotName,
	                     APlayerController* PC, FName BonfireId);

	/** 현재 플레이어 카메라와 같은 시점으로 UI 없는 16:9 PNG 썸네일을 만든다. */
	bool CaptureSaveThumbnail(APlayerController* PC, URetrieveSaveGame* SlotSave) const;

	/** 내부 공통 로드 로직. SlotName에서 읽어 플레이어에 적용 */
	bool ReadSaveFromSlot(const FString& SlotName, APlayerController* PC);

	/** 레거시 슬롯명 결정 (하위 호환) */
	FString GetSlotNameForPlayer(APlayerController* PC) const;

	UPROPERTY()
	TObjectPtr<URetrieveSaveGame> CurrentSaveGame;

	FTimerHandle FastTravelTimerHandle;
	FTimerHandle FastTravelStreamPollTimerHandle;
	FTimerHandle FastTravelFinishTimerHandle;

	UPROPERTY()
	TObjectPtr<APlayerController> PendingFastTravelPC;

	// ── 빠른 이동 도착 보류 상태 (스트리밍 폴링 콜백에서 사용) ──
	FName PendingFastTravelBonfireId;
	FTransform PendingFastTravelArrival;
	TWeakObjectPtr<APawn> PendingFastTravelPawn;
	TWeakObjectPtr<UCharacterMovementComponent> PendingFastTravelCharMove;
	bool bPendingFastTravelHadCollision = false;
	TEnumAsByte<EMovementMode> PendingFastTravelPrevMovementMode = MOVE_None;
	uint8 PendingFastTravelPrevCustomMode = 0;
	float FastTravelStreamElapsed = 0.0f;
	float FastTravelSettleElapsed = 0.0f;
	float FastTravelSettleStableElapsed = 0.0f;

	/** 목적지 셀 로딩을 유도하는 임시 World Partition 스트리밍 소스 액터/컴포넌트. */
	UPROPERTY()
	TObjectPtr<AActor> FastTravelStreamingSourceActor;
	TWeakObjectPtr<UWorldPartitionStreamingSourceComponent> FastTravelStreamingSourceComp;

	/** 불러오기 경로에서 SaveSubsystem이 직접 띄운 로딩화면 인스턴스. */
	UPROPERTY()
	TObjectPtr<UUserWidget> ActiveLoadingScreen;

	static constexpr int32 SaveUserIndex = 0;
	/** 월드 공유 상태 (화톳불 활성화) 전용 슬롯명 */
	static constexpr const TCHAR* WorldStateSlotName = TEXT("RetrieveSave_WorldState");
	/** 레거시 기본 슬롯명 (하위 호환) */
	static constexpr const TCHAR* DefaultSaveSlotName = TEXT("RetrieveSave_Slot0");
};
