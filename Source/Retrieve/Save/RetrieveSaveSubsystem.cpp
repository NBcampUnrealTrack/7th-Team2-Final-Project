#include "Save/RetrieveSaveSubsystem.h"
#include "Save/RetrieveSaveGame.h"
#include "Components/Inventory/InventoryComponent.h"
#include "Components/Combat/RetrieveHealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Misc/DateTime.h"
#include "Engine/World.h"
#include "Engine/HitResult.h"
#include "CollisionQueryParams.h"
#include "GameFramework/Actor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SceneComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/WorldPartitionStreamingSourceComponent.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "ContentStreaming.h"
#include "EngineUtils.h"
#include "Blueprint/UserWidget.h"
#include "World/RetrieveBonfireActor.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Core/RetrieveGameState.h"
#include "Subsystems/RetrieveMapSubsystem.h"
#include "Quest/QuestBranchComponent.h"
#include "UI/Quest/RetrieveQuestStatus.h"
#include "Subsystems/QuestNotificationSubsystem.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "GameplayTags/RetrieveGameplayTags.h"

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

	// 월드 공유 상태 복사 (WorldState에서)
	if (CurrentSaveGame)
	{
		SlotSave->ActivatedBonfireTransforms = CurrentSaveGame->ActivatedBonfireTransforms;
		SlotSave->UnlockedElements           = CurrentSaveGame->UnlockedElements;
		SlotSave->bLumenEngraved             = CurrentSaveGame->bLumenEngraved;
		SlotSave->ShopRepurchaseHistory      = CurrentSaveGame->ShopRepurchaseHistory;
		SlotSave->DialogueRewardGrantCounts  = CurrentSaveGame->DialogueRewardGrantCounts;
		SlotSave->RpsRewardGrantCounts       = CurrentSaveGame->RpsRewardGrantCounts;
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

	// 전장의 안개 탐색 마스크 저장 (라이브 MapSubsystem에서 직접 캡처)
	if (UWorld* World = PC->GetWorld())
	{
		if (URetrieveMapSubsystem* MapSub = World->GetSubsystem<URetrieveMapSubsystem>())
		{
			const TArray<uint8>& Mask = MapSub->GetRevealMaskData();
			if (Mask.Num() > 0)
			{
				SlotSave->ExploredMask = Mask;
				SlotSave->ExploredMaskResolution = MapSub->GetRevealMaskResolution();
			}
		}
	}

	// 퀘스트 진행 상태 기록 (GameState 소속이라 Pawn과 별도로 조회)
	SlotSave->TrackedQuestName = FString();
	SlotSave->TrackedQuestObjective = FString();
	if (UWorld* World = PC->GetWorld())
	{
		if (ARetrieveGameState* GS = World->GetGameState<ARetrieveGameState>())
		{
			if (UQuestBranchComponent* Quest = GS->GetQuestBranchComponent())
			{
				Quest->MakeQuestSaveData(SlotSave->CompletedQuestSteps, SlotSave->CurrentTrackerStep,
					SlotSave->QuestChoiceHistory);

				// UI 표시용: HUD 트래커(QuestTrackerViewModel::Recompute)와 동일한 기본 선정 로직으로
				// "지금 추적 중일 퀘스트"를 재계산한다. 어떤 퀘스트를 명시적으로 추적 중이었는지는
				// HUD의 로컬 상태라 세이브에 없으므로, 저장 시점엔 항상 이 기본값 기준으로 계산한다.
				if (const UDataTable* QuestTable = GS->GetQuestTable())
				{
					static const FString Ctx(TEXT("SaveSubsystem_ResolveTrackedQuest"));
					TArray<FQuestDefinition*> Rows;
					QuestTable->GetAllRows<FQuestDefinition>(Ctx, Rows);

					Rows.Sort([](const FQuestDefinition& A, const FQuestDefinition& B)
					{
						return A.DisplayOrder < B.DisplayOrder;
					});

					const FQuestDefinition* DefaultMain = nullptr;
					const FQuestDefinition* DefaultAny = nullptr;
					for (const FQuestDefinition* Row : Rows)
					{
						if (!Row || !QuestStatus::IsQuestUnlocked(*Row, *Quest) || QuestStatus::AreAllObjectivesComplete(*Row, *Quest))
						{
							continue;
						}
						if (!DefaultAny)
						{
							DefaultAny = Row;
						}
						if (!DefaultMain && Row->Type == EQuestType::Main)
						{
							DefaultMain = Row;
						}
					}

					if (const FQuestDefinition* Tracked = DefaultMain ? DefaultMain : DefaultAny)
					{
						SlotSave->TrackedQuestName = Tracked->DisplayName.ToString();
						for (const FQuestObjective& Obj : Tracked->Objectives)
						{
							if (!Quest->IsStepCompleted(Obj.CompletionTag))
							{
								SlotSave->TrackedQuestObjective = Obj.ObjectiveText.ToString();
								break;
							}
						}
					}
				}
			}
		}
	}

	// 저장 시각 기록
	SlotSave->SaveTimestamp = FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M"));

	// 플레이어에게 메뉴 깜빡임을 노출하지 않기 위해 뷰포트가 아닌 별도 SceneCapture를 사용한다.
	// 캡처 실패가 세이브 데이터 자체를 막아서는 안 되므로 실패 시 텍스트 저장은 계속 진행한다.
	CaptureSaveThumbnail(PC, SlotSave);

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

bool URetrieveSaveSubsystem::CaptureSaveThumbnail(APlayerController* PC, URetrieveSaveGame* SlotSave) const
{
	if (!IsValid(PC) || !SlotSave || !PC->PlayerCameraManager)
	{
		return false;
	}

	UWorld* World = PC->GetWorld();
	if (!World)
	{
		return false;
	}

	constexpr int32 ThumbnailWidth = 512;
	constexpr int32 ThumbnailHeight = 288;

	const FMinimalViewInfo CameraView = PC->PlayerCameraManager->GetCameraCacheView();

	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(
		GetTransientPackage(), NAME_None, RF_Transient);
	RenderTarget->RenderTargetFormat = RTF_RGBA8;
	RenderTarget->ClearColor = FLinearColor::Black;
	RenderTarget->InitAutoFormat(ThumbnailWidth, ThumbnailHeight);
	RenderTarget->UpdateResourceImmediate(true);

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASceneCapture2D* CaptureActor = World->SpawnActor<ASceneCapture2D>(
		CameraView.Location, CameraView.Rotation, SpawnParams);
	if (!CaptureActor)
	{
		return false;
	}

	USceneCaptureComponent2D* Capture = CaptureActor->GetCaptureComponent2D();
	Capture->TextureTarget = RenderTarget;
	Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	Capture->FOVAngle = CameraView.FOV;
	Capture->PostProcessSettings = CameraView.PostProcessSettings;
	Capture->PostProcessBlendWeight = CameraView.PostProcessBlendWeight;
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->bAlwaysPersistRenderingState = true;
	
	constexpr float ThumbnailExposureEV100 = -2.0f;
	Capture->PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
	Capture->PostProcessSettings.AutoExposureMinBrightness = ThumbnailExposureEV100;
	Capture->PostProcessSettings.bOverride_AutoExposureMaxBrightness = true;
	Capture->PostProcessSettings.AutoExposureMaxBrightness = ThumbnailExposureEV100;
	
	Capture->CaptureScene();

	TArray<FColor> Pixels;
	FReadSurfaceDataFlags ReadFlags(RCM_UNorm);
	ReadFlags.SetLinearToGamma(false);
	const bool bRead = RenderTarget->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, ReadFlags);
	CaptureActor->Destroy();

	if (!bRead || Pixels.Num() != ThumbnailWidth * ThumbnailHeight)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SaveSubsystem] 저장 썸네일 픽셀 읽기 실패"));
		return false;
	}

	// SCS_FinalColorLDR는 렌더러/포스트프로세스 조합에 따라 RGB는 정상이어도
	// 백버퍼 알파를 0으로 반환할 수 있다. 그대로 PNG로 저장하면 UMG에서 완전히
	// 투명하게 합성되어 검은 슬롯 배경만 보이므로 썸네일은 항상 불투명하게 만든다.
	for (FColor& Pixel : Pixels)
	{
		Pixel.A = 255;
	}

	SlotSave->ScreenshotPng.Reset();
	TArray64<uint8> CompressedPng;
	FImageUtils::PNGCompressImageArray(
		ThumbnailWidth, ThumbnailHeight, Pixels, CompressedPng);
	SlotSave->ScreenshotPng.Append(CompressedPng.GetData(), static_cast<int32>(CompressedPng.Num()));
	SlotSave->ScreenshotWidth = ThumbnailWidth;
	SlotSave->ScreenshotHeight = ThumbnailHeight;

	const bool bCaptured = !SlotSave->ScreenshotPng.IsEmpty();
	UE_LOG(LogTemp, Log, TEXT("[SaveSubsystem] 저장 썸네일 캡처 %s (%d bytes)"),
		bCaptured ? TEXT("완료") : TEXT("실패"), SlotSave->ScreenshotPng.Num());
	return bCaptured;
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

	// 퀘스트 진행 상태 복원. 개별 스텝 알림을 재생하지 않기 위해 알림 베이스라인을
	// 복원된 상태로 재시딩한 뒤, UI(트래커/퀘스트로그)에는 갱신 신호만 한 번 보낸다.
	if (UWorld* World = PC->GetWorld())
	{
		if (ARetrieveGameState* GS = World->GetGameState<ARetrieveGameState>())
		{
			if (UQuestBranchComponent* Quest = GS->GetQuestBranchComponent())
			{
				Quest->ApplyQuestSaveData(CurrentSaveGame->CompletedQuestSteps,
					CurrentSaveGame->CurrentTrackerStep, CurrentSaveGame->QuestChoiceHistory);
			}
		}

		if (UQuestNotificationSubsystem* NotificationSubsystem = World->GetSubsystem<UQuestNotificationSubsystem>())
		{
			NotificationSubsystem->ResetBaseline();
		}

		// 전장의 안개 탐색 마스크 복원 (bounds 초기화 전이라도 raw 마스크는 안전하게 세팅됨)
		if (URetrieveMapSubsystem* MapSub = World->GetSubsystem<URetrieveMapSubsystem>())
		{
			if (CurrentSaveGame->ExploredMaskResolution > 0)
			{
				MapSub->SetRevealMaskData(CurrentSaveGame->ExploredMask,
					CurrentSaveGame->ExploredMaskResolution);
			}
		}

		FRetrieveQuestStepPayload RefreshMessage;
		RefreshMessage.StepTag = CurrentSaveGame->CurrentTrackerStep;
		UGameplayMessageSubsystem::Get(World).BroadcastMessage(
			RetrieveGameplayTags::Channel_Quest_StepChanged, RefreshMessage);
	}

	const FRetrieveLoadSnapshotData& Snapshot = CurrentSaveGame->LoadSnapshot;
	// 저장된 위치로 텔레포트 — 빠른 이동과 동일하게 로딩화면을 띄우고
	// 목적지 스트리밍이 안정될 때까지 대기한 뒤 안착시킨다(직접 SetActorTransform 대신).
	// 저장된 정확한 플레이어 좌표를 쓰므로 화톳불 재계산은 하지 않는다(NAME_None).
	ShowLoadingScreen(PC);
	BeginStreamedTeleport(PC, Snapshot.PlayerTransform, NAME_None);

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

int32 URetrieveSaveSubsystem::GetMostRecentSaveSlotIndex() const
{
	int32 BestSlot = INDEX_NONE;
	FString BestTimestamp;

	for (int32 SlotIndex : GetExistingSaveSlotIndices())
	{
		if (URetrieveSaveGame* Save = GetSaveGameForSlot(SlotIndex))
		{
			// SaveTimestamp는 "%Y-%m-%d %H:%M" 형식이라 문자열 비교만으로 시간순 정렬이 성립한다.
			if (BestSlot == INDEX_NONE || Save->SaveTimestamp > BestTimestamp)
			{
				BestSlot = SlotIndex;
				BestTimestamp = Save->SaveTimestamp;
			}
		}
	}
	return BestSlot;
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

void URetrieveSaveSubsystem::RegisterDefaultBonfire(FName BonfireId, const FTransform& ArrivalTransform)
{
	if (BonfireId.IsNone()) { return; }

	if (!CurrentSaveGame)
	{
		CurrentSaveGame = Cast<URetrieveSaveGame>(
			UGameplayStatics::CreateSaveGameObject(URetrieveSaveGame::StaticClass()));
		CurrentSaveGame->SlotIndex = -1;
	}

	// 이미 활성 기록이 있으면(실제 세이브 등) 덮어쓰지 않는다 — 진행 상태 우선.
	if (CurrentSaveGame->ActivatedBonfireTransforms.Contains(BonfireId)) { return; }

	// 디스크 저장 없이 인메모리로만 등록 → 실제 세이브 파일 오염 방지.
	CurrentSaveGame->ActivatedBonfireTransforms.Emplace(BonfireId, ArrivalTransform);

}

int32 URetrieveSaveSubsystem::GetNpcRewardGrantCount(FGameplayTag SpeakerTag, bool bRpsBet) const
{
	if (!CurrentSaveGame || !SpeakerTag.IsValid())
	{
		return 0;
	}
	const TMap<FGameplayTag, int32>& Counts = bRpsBet
		? CurrentSaveGame->RpsRewardGrantCounts
		: CurrentSaveGame->DialogueRewardGrantCounts;
	const int32* Found = Counts.Find(SpeakerTag);
	return Found ? *Found : 0;
}

void URetrieveSaveSubsystem::IncrementNpcRewardGrantCount(FGameplayTag SpeakerTag, bool bRpsBet)
{
	if (!SpeakerTag.IsValid())
	{
		return;
	}

	if (!CurrentSaveGame)
	{
		CurrentSaveGame = Cast<URetrieveSaveGame>(
			UGameplayStatics::CreateSaveGameObject(URetrieveSaveGame::StaticClass()));
		CurrentSaveGame->SlotIndex = -1;
	}

	TMap<FGameplayTag, int32>& Counts = bRpsBet
		? CurrentSaveGame->RpsRewardGrantCounts
		: CurrentSaveGame->DialogueRewardGrantCounts;
	Counts.FindOrAdd(SpeakerTag) += 1;

	// 보상 지급은 드문 이벤트이므로 즉시 WorldState 슬롯에 영속화한다(모닥불 활성화와 동일).
	UGameplayStatics::SaveGameToSlot(CurrentSaveGame, WorldStateSlotName, SaveUserIndex);
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

// ── 원소 해방 (월드 공유 진행 상태) ────────────────────────────────────────────

void URetrieveSaveSubsystem::MarkElementUnlocked(FGameplayTag ElementTag)
{
	if (!ElementTag.IsValid()) { return; }

	if (!CurrentSaveGame)
	{
		CurrentSaveGame = Cast<URetrieveSaveGame>(
			UGameplayStatics::CreateSaveGameObject(URetrieveSaveGame::StaticClass()));
		CurrentSaveGame->SlotIndex = -1;
	}

	if (CurrentSaveGame->UnlockedElements.HasTagExact(ElementTag))
	{
		return; // 중복
	}

	CurrentSaveGame->UnlockedElements.AddTag(ElementTag);

	// WorldState 슬롯에 자동 저장
	const bool bSaved = UGameplayStatics::SaveGameToSlot(
		CurrentSaveGame, WorldStateSlotName, SaveUserIndex);

	UE_LOG(LogTemp, Log,
		TEXT("[SaveSubsystem] 원소 해방 저장 — Element=%s WorldState=%s"),
		*ElementTag.ToString(), bSaved ? TEXT("OK") : TEXT("FAIL"));
}

bool URetrieveSaveSubsystem::IsElementUnlocked(FGameplayTag ElementTag) const
{
	return CurrentSaveGame && CurrentSaveGame->UnlockedElements.HasTagExact(ElementTag);
}

FGameplayTagContainer URetrieveSaveSubsystem::GetUnlockedElements() const
{
	return CurrentSaveGame ? CurrentSaveGame->UnlockedElements : FGameplayTagContainer();
}

void URetrieveSaveSubsystem::SetLumenEngraved(bool bEngraved)
{
	if (!CurrentSaveGame)
	{
		CurrentSaveGame = Cast<URetrieveSaveGame>(
			UGameplayStatics::CreateSaveGameObject(URetrieveSaveGame::StaticClass()));
		CurrentSaveGame->SlotIndex = -1;
	}

	if (CurrentSaveGame->bLumenEngraved == bEngraved)
	{
		return;
	}

	CurrentSaveGame->bLumenEngraved = bEngraved;

	// WorldState 슬롯에 자동 저장
	const bool bSaved = UGameplayStatics::SaveGameToSlot(
		CurrentSaveGame, WorldStateSlotName, SaveUserIndex);

	UE_LOG(LogTemp, Log,
		TEXT("[SaveSubsystem] 루멘 각인 상태 저장 — %s WorldState=%s"),
		bEngraved ? TEXT("true") : TEXT("false"), bSaved ? TEXT("OK") : TEXT("FAIL"));
}

bool URetrieveSaveSubsystem::IsLumenEngraved() const
{
	return CurrentSaveGame && CurrentSaveGame->bLumenEngraved;
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

	// 빠른 이동 목적지도 리스폰 체크포인트로 기록한다("마지막 사용 모닥불" 의미).
	// (기존에는 휴식 시에만 기록되어, 빠른 이동만 한 뒤 죽으면 시작 지역으로 리스폰되는 문제가 있었다.
	//  SetLastCheckpointBonfire는 호스트 권한에서만 반영된다 — 싱글/호스트 기준.)
	if (UWorld* World = GetWorld())
	{
		if (ARetrieveGameState* GS = World->GetGameState<ARetrieveGameState>())
		{
			GS->SetLastCheckpointBonfire(BonfireId);
		}
	}

	// 빠른 이동: 도착 화톳불의 ArrivalPoint로 재계산하도록 BonfireId 전달.
	// (오버레이는 호출 측 WorldMapWidget이 표시하므로 여기서는 별도 표시 안 함.)
	BeginStreamedTeleport(PC, ArrivalTransform, BonfireId);
}

// 빠른 이동 / 불러오기 공용: 도착 위치로 텔레포트하고 스트리밍이 안정될 때까지 대기한다.
// BonfireIdForRecompute가 유효하면 도착 후 살아있는 화톳불 ArrivalPoint로 위치를 재계산한다.
// (불러오기는 저장된 정확한 좌표를 쓰므로 NAME_None을 넘긴다.)
void URetrieveSaveSubsystem::BeginStreamedTeleport(APlayerController* PC, const FTransform& ArrivalTransform, FName BonfireIdForRecompute)
{
	if (!IsValid(PC)) { return; }

	APawn* Pawn = PC->GetPawn();
	if (!IsValid(Pawn))
	{
		UE_LOG(LogTemp, Warning, TEXT("[SaveSubsystem] BeginStreamedTeleport: Pawn 없음"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SaveSubsystem] BeginStreamedTeleport: World 없음"));
		return;
	}

	PendingFastTravelPC = PC;
	OnFastTravelStarted.Broadcast(BonfireIdForRecompute);

	if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent())
	{
		Movement->StopMovementImmediately();
	}

	// World Partition 대응 버전:
	// 도착 셀을 미리 로드하기 위해 "도착 위치에 임시 스트리밍 소스"를 등록하고,
	// 해당 소스의 스트리밍이 완료(또는 타임아웃)될 때까지 폴링한 뒤 한 번만 텔레포트한다.
	// 이렇게 하면 로딩 화면이 떠 있는 동안 목적지가 충분히 로드되어
	// "캐릭터가 화면에서 이동하다 모닥불 위로 떨어지는" 현상이 사라진다.
	ACharacter* Character = Cast<ACharacter>(Pawn);
	UCharacterMovementComponent* CharMove = Character ? Character->GetCharacterMovement() : nullptr;

	// 도착 시 복원할 상태를 멤버에 저장 (폴링 콜백에서 사용).
	PendingFastTravelBonfireId        = BonfireIdForRecompute;
	PendingFastTravelArrival          = ArrivalTransform;
	PendingFastTravelPawn             = Pawn;
	bPendingFastTravelHadCollision    = Pawn->GetActorEnableCollision();
	PendingFastTravelCharMove         = CharMove;
	PendingFastTravelPrevMovementMode = MOVE_None;
	PendingFastTravelPrevCustomMode   = 0;

	if (CharMove)
	{
		PendingFastTravelPrevMovementMode = CharMove->MovementMode;
		PendingFastTravelPrevCustomMode   = CharMove->CustomMovementMode;
		CharMove->StopMovementImmediately();
		// MOVE_None 상태에서는 중력이 적용되지 않으므로 지면이 없어도 떨어지지 않는다.
		CharMove->DisableMovement();
	}

	// 복원 목표 정화: 사망 직후 리스폰처럼 이동/충돌이 일시적으로 꺼진 순간에 캡처되면
	// 도착 후 MOVE_None·충돌 꺼짐이 "복원"되어 조작 불능이나 지면 통과(땅꺼짐)가 된다.
	// (죽음 처리 HandleDeathStarted가 DisableMovement를 걸고, ALS 래그돌 복구 타이밍과 경합)
	if (PendingFastTravelPrevMovementMode == MOVE_None)
	{
		PendingFastTravelPrevMovementMode = MOVE_Walking;
	}
	bPendingFastTravelHadCollision = true;

	// 스트리밍/지면 스냅이 끝날 때까지 물리/충돌로 인한 밀림·낙하를 막는다.
	Pawn->SetActorEnableCollision(false);

	// 시작 즉시 목적지로 텔레포트한다.
	// 이동/충돌을 끈 상태라 지면이 아직 로드되지 않아도 그 자리에 고정되어 떨어지지 않으며,
	// 로딩 오버레이 타이밍과 무관하게 플레이어가 출발지에 보이지 않는다.
	// 목적지 셀은 (a)플레이어 본인 스트리밍 소스 (b)아래 임시 소스 양쪽으로 로드된다.
	Pawn->SetActorTransform(ArrivalTransform, false, nullptr, ETeleportType::TeleportPhysics);
	if (PC->PlayerCameraManager)
	{
		// 카메라가 출발지→목적지로 블렌딩(축지법)하지 않도록 이번 프레임 컷을 강제한다.
		PC->PlayerCameraManager->SetGameCameraCutThisFrame();
	}

	// 도착 위치에 임시 World Partition 스트리밍 소스를 스폰 → 목적지 셀 로딩을 유도한다.
	CleanupFastTravelStreamingSource();
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.ObjectFlags |= RF_Transient;

		AActor* SourceActor = World->SpawnActor<AActor>(
			AActor::StaticClass(), ArrivalTransform.GetLocation(), FRotator::ZeroRotator, SpawnParams);
		if (SourceActor)
		{
			USceneComponent* Root = NewObject<USceneComponent>(SourceActor, TEXT("StreamSourceRoot"));
			SourceActor->SetRootComponent(Root);
			Root->RegisterComponent();
			SourceActor->SetActorLocation(ArrivalTransform.GetLocation());

			UWorldPartitionStreamingSourceComponent* SourceComp =
				NewObject<UWorldPartitionStreamingSourceComponent>(SourceActor, TEXT("FastTravelStreamSource"));
			SourceComp->RegisterComponent();

			FastTravelStreamingSourceActor = SourceActor;
			FastTravelStreamingSourceComp  = SourceComp;
		}
	}

	// 현재 위치에서 가능한 스트리밍 요청을 먼저 처리한다.
	World->FlushLevelStreaming(EFlushLevelStreamingType::Full);

	UE_LOG(LogTemp, Log,
		TEXT("[SaveSubsystem] StreamedTeleport 준비 — BonfireId=%s Arrival=%s (목적지 스트리밍 대기 시작)"),
		*BonfireIdForRecompute.ToString(),
		*ArrivalTransform.GetLocation().ToString());

	// 고정 지연이 아니라 "목적지 스트리밍 완료"를 폴링한다.
	FastTravelStreamElapsed = 0.0f;
	World->GetTimerManager().SetTimer(
		FastTravelStreamPollTimerHandle,
		this,
		&URetrieveSaveSubsystem::PollFastTravelStreaming,
		FMath::Max(FastTravelStreamPollInterval, 0.02f),
		/*bLoop*/ true);
}

void URetrieveSaveSubsystem::PollFastTravelStreaming()
{
	FastTravelStreamElapsed += FMath::Max(FastTravelStreamPollInterval, 0.02f);

	// 소스가 막 등록된 직후에는 WP가 아직 필요 셀을 계산하지 못해 완료로 보일 수 있으므로
	// 최소 대기 시간을 둔 뒤에만 완료를 인정한다. 타임아웃 시에는 강제로 진행한다.
	const bool bMinWaitPassed = FastTravelStreamElapsed >= FastTravelStreamMinWait;
	const bool bTimedOut      = FastTravelStreamElapsed >= FastTravelStreamTimeout;

	bool bStreamingDone = false;
	if (UWorldPartitionStreamingSourceComponent* SourceComp = FastTravelStreamingSourceComp.Get())
	{
		bStreamingDone = SourceComp->IsStreamingCompleted();
	}
	else
	{
		// 스트리밍 소스가 없는(비 WP) 월드 — 최소 대기만 충족하면 진행한다.
		bStreamingDone = true;
	}

	if (bTimedOut || (bMinWaitPassed && bStreamingDone))
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(FastTravelStreamPollTimerHandle);
		}

		if (bTimedOut && !bStreamingDone)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[SaveSubsystem] FastTravel 스트리밍 타임아웃(%.1fs) — 강제 진행. BonfireId=%s"),
				FastTravelStreamElapsed, *PendingFastTravelBonfireId.ToString());
		}

		PerformFastTravelArrival();
	}
}

void URetrieveSaveSubsystem::PerformFastTravelArrival()
{
	// 도착점보다 너무 높은 곳에서 트레이스를 시작하면(예: 동굴 지붕 위 지형) 아래로 쏠 때
	// 동굴 바닥이 아니라 동굴 위 지형의 윗면을 먼저 맞혀 그 위로 스냅되는 문제가 있었다.
	// 시작 높이를 낮춰(400) 동굴/오버행 안쪽(지붕 아래)에서 트레이스가 시작되도록 한다.
	constexpr float GroundTraceUpHeight  = 400.0f;
	constexpr float GroundTraceDownDepth = 12000.0f;
	constexpr float GroundClearance      = 3.0f;
	constexpr float MinWalkableNormalZ   = 0.35f;

	UWorld* World = GetWorld();
	APawn* Pawn = PendingFastTravelPawn.Get();
	APlayerController* PC = PendingFastTravelPC;
	const FName BonfireId = PendingFastTravelBonfireId;

	if (!World || !IsValid(Pawn))
	{
		CleanupFastTravelStreamingSource();
		FinishFastTravel();
		return;
	}

	// 목적지 셀을 동기 로드 완료시킨다 (스트리밍 소스가 등록된 상태이므로 실제로 로드됨).
	World->FlushLevelStreaming(EFlushLevelStreamingType::Full);

	// 스트리밍 완료 후에는 목적지 모닥불 액터가 로드돼 있으므로, 세이브에 박제된 옛 좌표 대신
	// 살아있는 액터의 현재 ArrivalPoint를 사용한다. → ArrivalPoint를 옮기면 세이브 삭제 없이 즉시 반영.
	// 불러오기(BonfireId=None)는 저장된 정확한 플레이어 좌표를 그대로 쓴다(재계산 안 함).
	FTransform ArrivalBase = PendingFastTravelArrival;
	if (!BonfireId.IsNone())
	{
		for (TActorIterator<ARetrieveBonfireActor> It(World); It; ++It)
		{
			if (It->BonfireId == BonfireId)
			{
				if (It->ArrivalPoint)
				{
					ArrivalBase = It->ArrivalPoint->GetComponentTransform();
				}
				break;
			}
		}
	}

	FTransform SafeTransform = ArrivalBase;
	const FVector ArrivalLocation = ArrivalBase.GetLocation();
	const FVector TraceStart = ArrivalLocation + FVector(0.0f, 0.0f, GroundTraceUpHeight);
	const FVector TraceEnd   = ArrivalLocation - FVector(0.0f, 0.0f, GroundTraceDownDepth);

	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(FastTravelGroundSnap), /*bTraceComplex*/ false);
	TraceParams.AddIgnoredActor(Pawn);
	if (IsValid(PC))
	{
		TraceParams.AddIgnoredActor(PC);
	}

	// 채널 트레이스(Visibility)를 쓴다. 이전의 ObjectType=WorldStatic 필터는 캠프 구조물처럼
	// WorldStatic이 아닌 바닥 메시(예: Wind 캠프의 HISM 데크)를 통과해 그 아래 지형에 스냅했고,
	// 플레이어가 구조물 속에 ~1m 파묻힌 채 배치되는 "리스폰/빠른이동 땅꺼짐"의 원인이었다.
	// (상호작용 존 스피어 등 오버랩류는 아래의 bBlockingHit 필터가 걸러낸다.)
	TArray<FHitResult> GroundHits;
	const bool bHit = World->LineTraceMultiByChannel(
		GroundHits, TraceStart, TraceEnd, ECC_Visibility, TraceParams);

	bool bFoundGround = false;
	FHitResult BestGroundHit;
	if (bHit)
	{
		for (const FHitResult& Hit : GroundHits)
		{
			if (!Hit.bBlockingHit)
			{
				continue;
			}
			// 벽/절벽 측면/얇은 장식물에 스냅되는 것을 줄이기 위해 위를 향한 표면만 후보로 사용한다.
			if (Hit.ImpactNormal.Z < MinWalkableNormalZ)
			{
				continue;
			}
			BestGroundHit = Hit;
			bFoundGround = true;
			break;
		}
	}

	if (bFoundGround)
	{
		// 주의: 스트리밍 텔레포트 동안 액터 충돌이 꺼져 있어(GetActorEnableCollision=false)
		// GetSimpleCollisionHalfHeight()가 0을 반환한다 — 캐릭터 "중심"이 지면 위 3cm에 놓여
		// 캡슐 반높이만큼(약 90cm) 파묻히던 리스폰/빠른이동 땅꺼짐의 진짜 원인.
		// 캡슐 컴포넌트에서 직접 읽는다.
		float HalfHeight = Pawn->GetSimpleCollisionHalfHeight();
		if (const ACharacter* PawnCharacter = Cast<ACharacter>(Pawn))
		{
			if (const UCapsuleComponent* Capsule = PawnCharacter->GetCapsuleComponent())
			{
				HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
			}
		}
		FVector SnappedLocation = ArrivalLocation;
		SnappedLocation.Z = BestGroundHit.ImpactPoint.Z + HalfHeight + GroundClearance;
		SafeTransform.SetLocation(SnappedLocation);

		UE_LOG(LogTemp, Log,
			TEXT("[SaveSubsystem] FastTravel 지면 스냅 성공(v2/채널트레이스) — BonfireId=%s HitActor=%s HitComp=%s ImpactZ=%.1f Final=%s NormalZ=%.2f"),
			*BonfireId.ToString(),
			*GetNameSafe(BestGroundHit.GetActor()),
			*GetNameSafe(BestGroundHit.GetComponent()),
			BestGroundHit.ImpactPoint.Z,
			*SnappedLocation.ToString(),
			BestGroundHit.ImpactNormal.Z);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[SaveSubsystem] FastTravel 지면 스냅 실패 — BonfireId=%s Arrival=%s HitCount=%d. 원본 Transform 사용"),
			*BonfireId.ToString(), *ArrivalLocation.ToString(), GroundHits.Num());
	}

	const bool bMoved = Pawn->SetActorTransform(
		SafeTransform, false, nullptr, ETeleportType::TeleportPhysics);

	// 최종 위치 기준으로 플레이어 본인 스트리밍 소스가 요구하는 셀을 한 번 더 동기 로드한다.
	// (검은 오버레이가 떠 있는 동안 처리되어 화면이 걷힌 뒤 팝인이 줄어든다.)
	World->FlushLevelStreaming(EFlushLevelStreamingType::Full);

	Pawn->SetActorEnableCollision(bPendingFastTravelHadCollision);

	if (UCharacterMovementComponent* CharMove = PendingFastTravelCharMove.Get())
	{
		CharMove->StopMovementImmediately();
		if (PendingFastTravelPrevMovementMode == MOVE_Custom)
		{
			CharMove->SetMovementMode(MOVE_Custom, PendingFastTravelPrevCustomMode);
		}
		else if (PendingFastTravelPrevMovementMode != MOVE_None)
		{
			CharMove->SetMovementMode(PendingFastTravelPrevMovementMode);
		}
		else
		{
			CharMove->SetMovementMode(MOVE_Walking);
		}
	}

	// 주의: 여기서는 카메라 컷을 하지 않는다.
	// 시작 텔레포트에서 이미 컷했고, 이 최종 스냅은 수 유닛(Z) 이동에 불과하다.
	// 여기서 다시 컷하면 그동안 검은 화면 뒤에서 누적된 라이팅/노출/Lumen/포그 수렴이
	// 리셋되어, 오버레이가 걷힐 때 라이팅이 덜 잡힌(밝은) 화면이 보인다.

	// 임시 스트리밍 소스 정리. 플레이어 본인 소스가 목적지로 옮겨졌으므로 더 이상 필요 없다.
	CleanupFastTravelStreamingSource();

	UE_LOG(LogTemp, Log,
		TEXT("[SaveSubsystem] FastTravel 최종 텔레포트 — BonfireId=%s Success=%s Final=%s"),
		*BonfireId.ToString(),
		bMoved ? TEXT("true") : TEXT("false"),
		*SafeTransform.GetLocation().ToString());

	// 셀 로딩이 끝나도 텍스처/메시 LOD가 스트리밍되는 동안 화면을 걷으면 팝인이 보인다.
	// 레벨 + 리소스 스트리밍이 모두 안정될 때까지 검은 오버레이를 유지하는 폴링을 시작한다.
	FastTravelSettleElapsed = 0.0f;
	FastTravelSettleStableElapsed = 0.0f;
	World->GetTimerManager().SetTimer(
		FastTravelFinishTimerHandle,
		this,
		&URetrieveSaveSubsystem::PollFastTravelSettle,
		0.1f,
		/*bLoop*/ true);
}

void URetrieveSaveSubsystem::PollFastTravelSettle()
{
	FastTravelSettleElapsed += 0.1f;

	UWorld* World = GetWorld();
	if (!World)
	{
		FinishFastTravel();
		return;
	}

	// 1) 레벨(World Partition 셀) 스트리밍 — 플레이어 본인 소스 기준 전체 완료 여부.
	World->FlushLevelStreaming(EFlushLevelStreamingType::Full);
	bool bLevelDone = true;
	if (UWorldPartitionSubsystem* WPSubsystem = World->GetSubsystem<UWorldPartitionSubsystem>())
	{
		bLevelDone = WPSubsystem->IsStreamingCompleted(nullptr);
	}

	// 2) 텍스처/메시 LOD 스트리밍 — 목적지 기준 desired mip이 계산되도록 최소 렌더 시간을 준 뒤,
	//    매 틱 소량(블록 시간 제한)씩 강제 스트리밍하며 남은 요청이 0이 되길 기다린다.
	bool bResourcesDone = false;
	if (FastTravelSettleElapsed >= FastTravelSettleMinRenderTime)
	{
		const int32 PendingRequests = IStreamingManager::Get().StreamAllResources(0.05f);
		bResourcesDone = (PendingRequests <= 0);
	}

	// 텔레포트 직후엔 렌더러/WP가 아직 목적지 주변의 요청을 "생성"조차 안 한 상태라
	// 첫 틱의 "완료"는 "요청 없음 = 완료"인 오판일 수 있다. 따라서 완료 상태가
	// 일정 시간(FastTravelSettleStableDuration) 연속 유지될 때만 진짜 완료로 인정한다.
	// 도중에 새 요청이 뜨면(아직 로딩 중) 안정 타이머를 리셋한다.
	const bool bComplete = bLevelDone && bResourcesDone;
	if (FastTravelSettleElapsed >= FastTravelSettleMinRenderTime && bComplete)
	{
		FastTravelSettleStableElapsed += 0.1f;
	}
	else
	{
		FastTravelSettleStableElapsed = 0.0f;
	}

	const bool bStableEnough = FastTravelSettleStableElapsed >= FastTravelSettleStableDuration;
	const bool bTimedOut = FastTravelSettleElapsed >= FastTravelSettleTimeout;

	if (bTimedOut || bStableEnough)
	{
		World->GetTimerManager().ClearTimer(FastTravelFinishTimerHandle);

		if (bTimedOut && !bStableEnough)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[SaveSubsystem] FastTravel settle 타임아웃(%.1fs) — Level=%s Resources=%s. 강제로 오버레이 해제."),
				FastTravelSettleElapsed,
				bLevelDone ? TEXT("done") : TEXT("pending"),
				bResourcesDone ? TEXT("done") : TEXT("pending"));
		}
		else
		{
			UE_LOG(LogTemp, Log,
				TEXT("[SaveSubsystem] FastTravel settle 완료(%.1fs, 안정 %.1fs) — 오버레이 해제."),
				FastTravelSettleElapsed, FastTravelSettleStableElapsed);
		}

		FinishFastTravel();
	}
}

void URetrieveSaveSubsystem::CleanupFastTravelStreamingSource()
{
	if (AActor* SourceActor = FastTravelStreamingSourceActor)
	{
		SourceActor->Destroy();
	}
	FastTravelStreamingSourceActor = nullptr;
	FastTravelStreamingSourceComp = nullptr;
}

void URetrieveSaveSubsystem::ShowLoadingScreen(APlayerController* PC)
{
	if (!IsValid(PC)) { return; }
	if (IsValid(ActiveLoadingScreen) && ActiveLoadingScreen->IsInViewport())
	{
		return; // 이미 표시 중
	}

	UClass* WidgetClass = LoadingScreenWidgetClass.LoadSynchronous();
	if (!WidgetClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[SaveSubsystem] ShowLoadingScreen: LoadingScreenWidgetClass 미설정 — 로딩화면 생략"));
		return;
	}

	ActiveLoadingScreen = CreateWidget<UUserWidget>(PC, WidgetClass);
	if (ActiveLoadingScreen)
	{
		ActiveLoadingScreen->AddToViewport(200);
	}
}

void URetrieveSaveSubsystem::HideLoadingScreen()
{
	if (IsValid(ActiveLoadingScreen))
	{
		ActiveLoadingScreen->RemoveFromParent();
	}
	ActiveLoadingScreen = nullptr;
}


void URetrieveSaveSubsystem::FinishFastTravel()
{
	// 어떤 경로로 완료되든 임시 스트리밍 소스/폴링 타이머가 남지 않도록 정리한다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FastTravelStreamPollTimerHandle);
		World->GetTimerManager().ClearTimer(FastTravelFinishTimerHandle);
	}
	CleanupFastTravelStreamingSource();
	PendingFastTravelPawn = nullptr;
	PendingFastTravelCharMove = nullptr;

	// 불러오기 경로에서 SaveSubsystem이 직접 띄운 로딩화면이 있으면 제거.
	// (빠른 이동은 WorldMapWidget이 띄운 별도 오버레이가 OnFastTravelCompleted로 스스로 제거됨)
	HideLoadingScreen();

	OnFastTravelCompleted.Broadcast();
	PendingFastTravelPC = nullptr;
	UE_LOG(LogTemp, Log, TEXT("[SaveSubsystem] StreamedTeleport 완료"));
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

void URetrieveSaveSubsystem::FlushWorldState()
{
	if (CurrentSaveGame)
	{
		UGameplayStatics::SaveGameToSlot(CurrentSaveGame, WorldStateSlotName, SaveUserIndex);
	}
}
