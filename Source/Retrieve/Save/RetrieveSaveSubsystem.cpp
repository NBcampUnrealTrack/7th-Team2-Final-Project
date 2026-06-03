#include "Save/RetrieveSaveSubsystem.h"
#include "Save/RetrieveSaveGame.h"
#include "Components/InventoryComponent.h"
#include "Components/RetrieveHealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PawnMovementComponent.h"
#include "TimerManager.h"
#include "Misc/DateTime.h"

void URetrieveSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// WorldState 슬롯 로드 (화톳불 활성화 정보 복원)
	if (UGameplayStatics::DoesSaveGameExist(WorldStateSlotName, SaveUserIndex))
	{
		CurrentSaveGame = Cast<URetrieveSaveGame>(
			UGameplayStatics::LoadGameFromSlot(WorldStateSlotName, SaveUserIndex));
		UE_LOG(LogTemp, Log,
			TEXT("[SaveSubsystem] WorldState 로드 완료 — ActivatedBonfires=%d"),
			CurrentSaveGame ? CurrentSaveGame->ActivatedBonfireTransforms.Num() : 0);
	}
	else if (UGameplayStatics::DoesSaveGameExist(DefaultSaveSlotName, SaveUserIndex))
	{
		// 레거시 Slot0 파일이 있으면 마이그레이션
		CurrentSaveGame = Cast<URetrieveSaveGame>(
			UGameplayStatics::LoadGameFromSlot(DefaultSaveSlotName, SaveUserIndex));
		UE_LOG(LogTemp, Log, TEXT("[SaveSubsystem] 레거시 Slot0 WorldState로 마이그레이션"));
	}
}

// ── 슬롯 이름 ──────────────────────────────────────────────────────────────────

FString URetrieveSaveSubsystem::GetSlotName(int32 SlotIndex)
{
	return FString::Printf(TEXT("RetrieveSave_Slot%d"), SlotIndex);
}

FString URetrieveSaveSubsystem::GetSlotNameForPlayer(APlayerController* /*PC*/) const
{
	return FString(DefaultSaveSlotName);
}

// ── 내부 공통 저장 / 로드 ──────────────────────────────────────────────────────

bool URetrieveSaveSubsystem::WriteSaveToSlot(URetrieveSaveGame* SlotSave,
                                              const FString& SlotName,
                                              APlayerController* PC,
                                              FName BonfireId)
{
	if (!SlotSave || !IsValid(PC) || BonfireId.IsNone()) { return false; }

	APawn* Pawn = PC->GetPawn();
	if (!IsValid(Pawn))
	{
		UE_LOG(LogTemp, Warning, TEXT("[SaveSubsystem] WriteSaveToSlot: Pawn 없음"));
		return false;
	}

	// 활성화된 화톳불 월드 상태 복사 (WorldState에서)
	if (CurrentSaveGame)
	{
		SlotSave->ActivatedBonfireTransforms = CurrentSaveGame->ActivatedBonfireTransforms;
	}

	// LoadSnapshot 기록
	FRetrieveLoadSnapshotData& Snapshot = SlotSave->LoadSnapshot;
	Snapshot.BonfireId       = BonfireId;
	Snapshot.PlayerTransform = Pawn->GetActorTransform();

	UInventoryComponent* Inventory = FindInventoryComponent(Pawn);
	if (Inventory)
	{
		Snapshot.EquippedWeaponId = Inventory->GetEquippedWeaponId();
		SlotSave->InventoryProgress.Inventory = Inventory->MakeInventorySaveData();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[SaveSubsystem] InventoryComponent 없음 — 위치/HP만 저장"));
		Snapshot.EquippedWeaponId = FName();
	}

	if (URetrieveHealthComponent* HealthComp =
		Pawn->FindComponentByClass<URetrieveHealthComponent>())
	{
		Snapshot.SavedHealth = HealthComp->GetHealth();
	}

	// 저장 시각 기록
	SlotSave->SaveTimestamp = FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M"));

	const bool bSuccess = UGameplayStatics::SaveGameToSlot(SlotSave, SlotName, SaveUserIndex);
	if (bSuccess)
	{
		OnSaveCompleted.Broadcast();
		UE_LOG(LogTemp, Log, TEXT("[SaveSubsystem] 저장 완료 — BonfireId=%s Slot=%s Time=%s"),
			*BonfireId.ToString(), *SlotName, *SlotSave->SaveTimestamp);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[SaveSubsystem] 파일 저장 실패 — Slot=%s"), *SlotName);
	}
	return bSuccess;
}

bool URetrieveSaveSubsystem::ReadSaveFromSlot(const FString& SlotName, APlayerController* PC)
{
	if (!IsValid(PC)) { return false; }

	if (!UGameplayStatics::DoesSaveGameExist(SlotName, SaveUserIndex))
	{
		UE_LOG(LogTemp, Log, TEXT("[SaveSubsystem] 저장 파일 없음 — Slot=%s"), *SlotName);
		return false;
	}

	URetrieveSaveGame* Loaded = Cast<URetrieveSaveGame>(
		UGameplayStatics::LoadGameFromSlot(SlotName, SaveUserIndex));
	if (!Loaded)
	{
		UE_LOG(LogTemp, Error, TEXT("[SaveSubsystem] 파일 로드 실패 — Slot=%s"), *SlotName);
		return false;
	}

	// CurrentSaveGame을 로드된 슬롯으로 교체 (화톳불 활성화 상태도 복원됨)
	CurrentSaveGame = Loaded;

	APawn* Pawn = PC->GetPawn();
	if (!IsValid(Pawn))
	{
		UE_LOG(LogTemp, Warning, TEXT("[SaveSubsystem] ReadSaveFromSlot: Pawn 없음"));
		return false;
	}

	UInventoryComponent* Inventory = FindInventoryComponent(Pawn);
	if (Inventory)
	{
		Inventory->ApplyInventorySaveData(CurrentSaveGame->InventoryProgress.Inventory);
	}

	const FRetrieveLoadSnapshotData& Snapshot = CurrentSaveGame->LoadSnapshot;
	Pawn->SetActorTransform(Snapshot.PlayerTransform, false, nullptr, ETeleportType::TeleportPhysics);

	if (URetrieveHealthComponent* HealthComp =
		Pawn->FindComponentByClass<URetrieveHealthComponent>())
	{
		// [GAS 연결 필요] HealthComp->SetHealth(Snapshot.SavedHealth);
		UE_LOG(LogTemp, Log, TEXT("[SaveSubsystem] 복원 HP=%.1f (GAS 연결 필요)"), Snapshot.SavedHealth);
	}

	OnLoadCompleted.Broadcast();
	UE_LOG(LogTemp, Log, TEXT("[SaveSubsystem] 로드 완료 — BonfireId=%s Slot=%s"),
		*Snapshot.BonfireId.ToString(), *SlotName);
	return true;
}

// ── 멀티슬롯 저장 / 로드 ───────────────────────────────────────────────────────

bool URetrieveSaveSubsystem::SaveToSlot(APlayerController* PC, FName BonfireId,
                                         int32 SlotIndex, const FString& BonfireDisplayName)
{
	if (!IsValidSaveSlot(SlotIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[SaveSubsystem] SaveToSlot: 유효하지 않은 SlotIndex=%d"), SlotIndex);
		return false;
	}

	URetrieveSaveGame* SlotSave = Cast<URetrieveSaveGame>(
		UGameplayStatics::CreateSaveGameObject(URetrieveSaveGame::StaticClass()));
	SlotSave->SlotIndex         = SlotIndex;
	SlotSave->BonfireDisplayName = BonfireDisplayName;

	const FString SlotName = GetSlotName(SlotIndex);
	const bool bSuccess = WriteSaveToSlot(SlotSave, SlotName, PC, BonfireId);
	if (bSuccess)
	{
		ActiveSlotIndex = SlotIndex;
	}
	return bSuccess;
}

bool URetrieveSaveSubsystem::LoadFromSlot(APlayerController* PC, int32 SlotIndex)
{
	if (!IsValidSaveSlot(SlotIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[SaveSubsystem] LoadFromSlot: 유효하지 않은 SlotIndex=%d"), SlotIndex);
		return false;
	}

	const FString SlotName = GetSlotName(SlotIndex);
	const bool bSuccess = ReadSaveFromSlot(SlotName, PC);
	if (bSuccess)
	{
		ActiveSlotIndex = SlotIndex;
	}
	return bSuccess;
}

URetrieveSaveGame* URetrieveSaveSubsystem::GetSaveGameForSlot(int32 SlotIndex) const
{
	if (!IsValidSaveSlot(SlotIndex)) { return nullptr; }
	const FString SlotName = GetSlotName(SlotIndex);
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, SaveUserIndex)) { return nullptr; }
	return Cast<URetrieveSaveGame>(
		UGameplayStatics::LoadGameFromSlot(SlotName, SaveUserIndex));
}

TArray<int32> URetrieveSaveSubsystem::GetExistingSaveSlotIndices() const
{
	TArray<int32> Result;
	for (int32 i = 0; i < MaxSaveSlots; ++i)
	{
		if (UGameplayStatics::DoesSaveGameExist(GetSlotName(i), SaveUserIndex))
		{
			Result.Add(i);
		}
	}
	return Result;
}

int32 URetrieveSaveSubsystem::GetNextFreeSlotIndex() const
{
	for (int32 i = 0; i < MaxSaveSlots; ++i)
	{
		if (!UGameplayStatics::DoesSaveGameExist(GetSlotName(i), SaveUserIndex))
		{
			return i;
		}
	}
	// 모든 슬롯이 찼으면 가장 오래된 Slot0 덮어쓰기
	return 0;
}

// ── 레거시 (하위 호환) ─────────────────────────────────────────────────────────

bool URetrieveSaveSubsystem::SaveAtBonfire(APlayerController* PC, FName BonfireId)
{
	const int32 NextSlot = GetNextFreeSlotIndex();
	return SaveToSlot(PC, BonfireId, NextSlot, TEXT(""));
}

bool URetrieveSaveSubsystem::LoadGame(APlayerController* PC)
{
	return LoadFromSlot(PC, 0);
}

bool URetrieveSaveSubsystem::HasSaveGame() const
{
	return !GetExistingSaveSlotIndices().IsEmpty();
}

bool URetrieveSaveSubsystem::HasSaveGameForPlayer(APlayerController* /*PC*/) const
{
	return HasSaveGame();
}

// ── 화톳불 활성화 ─────────────────────────────────────────────────────────────

void URetrieveSaveSubsystem::MarkBonfireActivated(FName BonfireId, FTransform ArrivalTransform,
                                                   const FString& /*BonfireDisplayName*/)
{
	if (BonfireId.IsNone()) { return; }

	if (!CurrentSaveGame)
	{
		CurrentSaveGame = Cast<URetrieveSaveGame>(
			UGameplayStatics::CreateSaveGameObject(URetrieveSaveGame::StaticClass()));
		CurrentSaveGame->SlotIndex = -1;
	}

	CurrentSaveGame->ActivatedBonfireTransforms.Emplace(BonfireId, ArrivalTransform);

	// WorldState 슬롯에 자동 저장
	const bool bSaved = UGameplayStatics::SaveGameToSlot(
		CurrentSaveGame, WorldStateSlotName, SaveUserIndex);

	UE_LOG(LogTemp, Log,
		TEXT("[SaveSubsystem] 화톳불 활성화 저장 — BonfireId=%s WorldState=%s"),
		*BonfireId.ToString(), bSaved ? TEXT("OK") : TEXT("FAIL"));
}

bool URetrieveSaveSubsystem::IsBonfireActivated(FName BonfireId) const
{
	if (!CurrentSaveGame || BonfireId.IsNone()) { return false; }
	return CurrentSaveGame->ActivatedBonfireTransforms.Contains(BonfireId);
}

bool URetrieveSaveSubsystem::GetBonfireTransform(FName BonfireId, FTransform& OutTransform) const
{
	if (!CurrentSaveGame || BonfireId.IsNone()) { return false; }
	if (const FTransform* Found = CurrentSaveGame->ActivatedBonfireTransforms.Find(BonfireId))
	{
		OutTransform = *Found;
		return true;
	}
	return false;
}

// ── 빠른 이동 (World Partition) ───────────────────────────────────────────────

void URetrieveSaveSubsystem::FastTravelToBonfire(FName BonfireId, APlayerController* PC)
{
	if (!IsValid(PC) || BonfireId.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[SaveSubsystem] FastTravel: 유효하지 않은 인자"));
		return;
	}

	FTransform ArrivalTransform;
	if (!GetBonfireTransform(BonfireId, ArrivalTransform))
	{
		UE_LOG(LogTemp, Warning, TEXT("[SaveSubsystem] FastTravel: %s 화톳불 Transform 없음"),
			*BonfireId.ToString());
		return;
	}

	APawn* Pawn = PC->GetPawn();
	if (!IsValid(Pawn))
	{
		UE_LOG(LogTemp, Warning, TEXT("[SaveSubsystem] FastTravel: Pawn 없음"));
		return;
	}

	PendingFastTravelPC = PC;
	OnFastTravelStarted.Broadcast(BonfireId);

	if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent())
	{
		Movement->StopMovementImmediately();
	}

	const FTransform SafeTransform = ArrivalTransform;

	const bool bMoved = Pawn->SetActorTransform(
		SafeTransform, false, nullptr, ETeleportType::TeleportPhysics);
	UE_LOG(LogTemp, Log, TEXT("[SaveSubsystem] FastTravel 텔레포트 — BonfireId=%s Success=%s"),
		*BonfireId.ToString(), bMoved ? TEXT("true") : TEXT("false"));

	UWorld* World = GetWorld();
	if (!World)
	{
		FinishFastTravel();
		return;
	}

	World->GetTimerManager().SetTimer(
		FastTravelTimerHandle,
		this,
		&URetrieveSaveSubsystem::FinishFastTravel,
		FastTravelStreamDelay,
		false);
}

void URetrieveSaveSubsystem::FinishFastTravel()
{
	OnFastTravelCompleted.Broadcast();
	PendingFastTravelPC = nullptr;
	UE_LOG(LogTemp, Log, TEXT("[SaveSubsystem] 빠른 이동 완료"));
}

// ── 내부 유틸 ────────────────────────────────────────────────────────────────

UInventoryComponent* URetrieveSaveSubsystem::FindInventoryComponent(AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UInventoryComponent>() : nullptr;
}

bool URetrieveSaveSubsystem::IsValidSaveSlot(int32 SlotIndex)
{
	return SlotIndex >= 0 && SlotIndex < MaxSaveSlots;
}
