#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Save/RetrieveSaveGame.h"
#include "RetrieveSaveSubsystem.generated.h"

class UInventoryComponent;

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
UCLASS(BlueprintType)
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

	// ── 빠른 이동 (World Partition) ───────────────────────────────────

	UFUNCTION(BlueprintCallable, Category = "Retrieve|FastTravel")
	void FastTravelToBonfire(FName BonfireId, APlayerController* PC);

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|FastTravel")
	float FastTravelStreamDelay = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|FastTravel")
	FVector FastTravelArrivalOffset = FVector(0.f, 0.f, 50.f);

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

	static UInventoryComponent* FindInventoryComponent(AActor* Actor);
	static bool IsValidSaveSlot(int32 SlotIndex);

	/** 내부 공통 저장 로직. SlotSave 오브젝트를 채우고 지정 슬롯명에 씀 */
	bool WriteSaveToSlot(URetrieveSaveGame* SlotSave, const FString& SlotName,
	                     APlayerController* PC, FName BonfireId);

	/** 내부 공통 로드 로직. SlotName에서 읽어 플레이어에 적용 */
	bool ReadSaveFromSlot(const FString& SlotName, APlayerController* PC);

	/** 레거시 슬롯명 결정 (하위 호환) */
	FString GetSlotNameForPlayer(APlayerController* PC) const;

	UPROPERTY()
	TObjectPtr<URetrieveSaveGame> CurrentSaveGame;

	FTimerHandle FastTravelTimerHandle;

	UPROPERTY()
	TObjectPtr<APlayerController> PendingFastTravelPC;

	static constexpr int32 SaveUserIndex = 0;
	/** 월드 공유 상태 (화톳불 활성화) 전용 슬롯명 */
	static constexpr const TCHAR* WorldStateSlotName = TEXT("RetrieveSave_WorldState");
	/** 레거시 기본 슬롯명 (하위 호환) */
	static constexpr const TCHAR* DefaultSaveSlotName = TEXT("RetrieveSave_Slot0");
};
