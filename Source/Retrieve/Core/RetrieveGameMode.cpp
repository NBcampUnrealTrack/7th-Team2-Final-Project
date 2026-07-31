#include "Core/RetrieveGameMode.h"

#include "Diagnostics/RetrieveDiagLog.h"
#include "RetrieveGameState.h"
#include "Character/LumenCharacter.h"
#include "Character/RetrieveAlsCombatCharacter.h"
#include "Components/Combat/RetrieveHealthComponent.h"
#include "Data/RetrieveOpeningSequenceAsset.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Messaging/GameplayMessages/RetrieveGameplayMessageTypes.h"
#include "Player/RetrievePlayerController.h"
#include "Player/RetrievePlayerState.h"
#include "Quest/QuestBranchComponent.h"
#include "Save/RetrieveSaveSubsystem.h"
#include "Subsystems/QuestNotificationSubsystem.h"
#include "Subsystems/RetrieveCinematicSubsystem.h"
#include "Subsystems/SystemMessageSubsystem.h"
#include "UI/Menu/RetrieveCreditsWidget.h"
#include "UObject/UObjectGlobals.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "World/RetrieveRescueEncounter.h"
#include "World/RetrieveLostCargoEncounter.h"
#include "World/RetrieveQuestEncounter.h"

ARetrieveGameMode::ARetrieveGameMode(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	RetrieveDiagCheckpoint(TEXT("GameMode constructor (CDO or instance)"));

	static bool bBoundPostLoadMapDiag = false;
	if (!bBoundPostLoadMapDiag)
	{
		bBoundPostLoadMapDiag = true;
		FCoreUObjectDelegates::PostLoadMapWithWorld.AddStatic(
			[](UWorld* World)
			{
				RetrieveDiagCheckpoint(*FString::Printf(TEXT("PostLoadMapWithWorld: %s"),
					World ? *World->GetName() : TEXT("null")));
			});
	}

	PlayerControllerClass = ARetrievePlayerController::StaticClass();
	PlayerStateClass = ARetrievePlayerState::StaticClass();
	GameStateClass = ARetrieveGameState::StaticClass();
}

void ARetrieveGameMode::PostLogin(APlayerController* NewPlayerController)
{
	RetrieveDiagCheckpoint(TEXT("GameMode::PostLogin start"));
	Super::PostLogin(NewPlayerController);

	ARetrieveGameState* GS = GetRetrieveGameState();
	if (!GS || !NewPlayerController)
	{
		return;
	}

	if (GS->GetHostPlayerState() == nullptr && NewPlayerController->PlayerState)
	{
		GS->SetHostPlayerState(NewPlayerController->PlayerState);
		RetrieveDiagCheckpoint(TEXT("GameMode::PostLogin -> OnWorldReadyForGameplay"));
		OnWorldReadyForGameplay();
	}
}

void ARetrieveGameMode::BeginPlay()
{
	RetrieveDiagCheckpoint(TEXT("GameMode::BeginPlay start"));
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		PlayerDiedListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FPlayerDiedPayload>(
			RetrieveGameplayTags::Channel_Player_Died, this, &ARetrieveGameMode::HandlePlayerDied);
		RevealGateListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FRetrieveRevealGatePayload>(
			RetrieveGameplayTags::Channel_UI_RevealGate, this, &ARetrieveGameMode::HandleRevealGate);
		QueenDefeatedListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FMonsterDiedPayload>(
			RetrieveGameplayTags::Channel_Game_QueenDefeated, this, &ARetrieveGameMode::HandleQueenDefeated);
		EndgameStepChangedListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FRetrieveQuestStepPayload>(
			RetrieveGameplayTags::Channel_Quest_StepChanged, this, &ARetrieveGameMode::HandleEndgameStepChanged);
	}
}

void ARetrieveGameMode::OnWorldReadyForGameplay()
{
	RetrieveDiagCheckpoint(TEXT("GameMode::OnWorldReadyForGameplay start"));
	ARetrieveGameState* GS = GetRetrieveGameState();
	if (!GS)
	{
		return;
	}

	GS->TransitionTo(ERetrieveSessionState::MainMenu);
	RetrieveDiagCheckpoint(TEXT("GameMode::OnWorldReadyForGameplay - MainMenu state set"));

	if (bDeveloperSkipIntroFlow || bSkipMainMenuOnBoot)
	{
		if (ARetrievePlayerController* RetrievePC = Cast<ARetrievePlayerController>(GetWorld()->GetFirstPlayerController()))
		{
			RetrievePC->SetDeveloperSkipIntroFlow(bDeveloperSkipIntroFlow);
		}
		BootstrapNewGameQuest();
		GS->TransitionTo(ERetrieveSessionState::InGame);
	}
	RetrieveDiagCheckpoint(TEXT("GameMode::OnWorldReadyForGameplay end"));
}

void ARetrieveGameMode::HandleNewGame(APlayerController* Requestor)
{
	if (!IsRequestorHost(Requestor))
	{
		return;
	}


	ResetNewGameState();
	ArmOpeningSequence(); // Awakening은 Reveal 이후 타이밍이 조절된 비트로 발생함 (StartOpeningSequence 참고)

	if (ARetrieveGameState* GS = GetRetrieveGameState())
	{
		GS->TransitionTo(ERetrieveSessionState::InGame);
	}
}

void ARetrieveGameMode::HandleContinueGame(APlayerController* Requestor)
{
	if (!IsRequestorHost(Requestor))
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	URetrieveSaveSubsystem* SaveSubsystem = GI ? GI->GetSubsystem<URetrieveSaveSubsystem>() : nullptr;
	if (!SaveSubsystem)
	{
		return;
	}

	const int32 SlotIndex = SaveSubsystem->GetMostRecentSaveSlotIndex();
	if (SlotIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] HandleContinueGame: 저장된 슬롯 없음"));
		return;
	}

	if (SaveSubsystem->LoadFromSlot(Requestor, SlotIndex))
	{
		if (ARetrieveGameState* GS = GetRetrieveGameState())
		{
			GS->TransitionTo(ERetrieveSessionState::InGame);
		}
	}
}

void ARetrieveGameMode::HandleLoadGameSlot(APlayerController* Requestor, int32 SlotIndex)
{
	if (!IsRequestorHost(Requestor))
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	URetrieveSaveSubsystem* SaveSubsystem = GI ? GI->GetSubsystem<URetrieveSaveSubsystem>() : nullptr;
	if (!SaveSubsystem)
	{
		return;
	}

	if (!SaveSubsystem->GetSaveGameForSlot(SlotIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] HandleLoadGameSlot: 슬롯 %d에 저장 데이터 없음"), SlotIndex);
		return;
	}

	if (SaveSubsystem->LoadFromSlot(Requestor, SlotIndex))
	{
		if (ARetrieveGameState* GS = GetRetrieveGameState())
		{
			GS->TransitionTo(ERetrieveSessionState::InGame);
		}
	}
}

void ARetrieveGameMode::HandleRetry(APlayerController* Requestor)
{
	if (!IsRequestorHost(Requestor))
	{
		return;
	}

	ARetrieveGameState* GS = GetRetrieveGameState();
	if (!GS)
	{
		return;
	}

	// 순서 중요: InGame 전환을 부활보다 먼저 한다. 반대 순서(부활 → InGame)에서는 부활 직후
	// 잔류 위협으로 재사망하면 HandlePlayerDied의 TransitionTo(Result)가 "아직 Result 상태"라
	// 동일 상태 전환으로 무시되고, 곧이어 InGame으로 넘어가 "죽은 폰 + InGame"(조작 가능한 시체)
	// 소프트락이 됐다. 전환을 먼저 하면 재사망 시 Result가 정상적으로 다시 뜬다.
	// 이 전환이 로딩 커버도 함께 띄우므로, 아래 CoverDelay 대기의 전제이기도 하다.
	GS->TransitionTo(ERetrieveSessionState::InGame);

	const FTransform RespawnTransform = GS->GetLastCheckpointOrFallback();

	UGameInstance* GI = GetGameInstance();
	URetrieveSaveSubsystem* SaveSubsystem = GI ? GI->GetSubsystem<URetrieveSaveSubsystem>() : nullptr;
	const float CoverDelay = SaveSubsystem ? SaveSubsystem->GetCoverFadeInSeconds() : 0.f;

	if (CoverDelay > KINDA_SMALL_NUMBER)
	{
		TWeakObjectPtr<APlayerController> WeakRequestor(Requestor);
		GetWorldTimerManager().SetTimer(
			RespawnCoverDelayTimerHandle,
			FTimerDelegate::CreateWeakLambda(this, [this, WeakRequestor, RespawnTransform]()
			{
				if (APlayerController* PC = WeakRequestor.Get())
				{
					RespawnPlayerAtTransform(PC, RespawnTransform);
				}
			}),
			CoverDelay, false);
	}
	else
	{
		RespawnPlayerAtTransform(Requestor, RespawnTransform);
	}
}

void ARetrieveGameMode::HandleQuitToMenu(APlayerController* Requestor)
{
	if (!IsRequestorHost(Requestor))
	{
		return;
	}

	if (ARetrieveGameState* GS = GetRetrieveGameState())
	{
		GS->TransitionTo(ERetrieveSessionState::MainMenu);
	}
}

void ARetrieveGameMode::HandleUnstuck(APlayerController* Requestor)
{
	if (!IsRequestorHost(Requestor))
	{
		return;
	}
	
	APawn* Pawn = Requestor ? Requestor->GetPawn() : nullptr;
	if (ARetrieveAlsCombatCharacter* CombatPawn = Cast<ARetrieveAlsCombatCharacter>(Pawn))
	{
		if (URetrieveHealthComponent* Health = CombatPawn->GetHealthComponent())
		{
			Health->KillOwner();
		}
	}
}

void ARetrieveGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (PlayerDiedListener.IsValid())
		{
			UGameplayMessageSubsystem::Get(World).UnregisterListener(PlayerDiedListener);
			PlayerDiedListener = FGameplayMessageListenerHandle();
		}
		if (RevealGateListener.IsValid())
		{
			UGameplayMessageSubsystem::Get(World).UnregisterListener(RevealGateListener);
			RevealGateListener = FGameplayMessageListenerHandle();
		}
		if (OpeningCinematicListener.IsValid())
		{
			UGameplayMessageSubsystem::Get(World).UnregisterListener(OpeningCinematicListener);
			OpeningCinematicListener = FGameplayMessageListenerHandle();
		}
		if (QueenDefeatedListener.IsValid())
		{
			UGameplayMessageSubsystem::Get(World).UnregisterListener(QueenDefeatedListener);
			QueenDefeatedListener = FGameplayMessageListenerHandle();
		}
		if (EndgameStepChangedListener.IsValid())
		{
			UGameplayMessageSubsystem::Get(World).UnregisterListener(EndgameStepChangedListener);
			EndgameStepChangedListener = FGameplayMessageListenerHandle();
		}
		if (EndgameCinematicListener.IsValid())
		{
			UGameplayMessageSubsystem::Get(World).UnregisterListener(EndgameCinematicListener);
			EndgameCinematicListener = FGameplayMessageListenerHandle();
		}
		World->GetTimerManager().ClearTimer(OpeningBeatTimer);
		World->GetTimerManager().ClearTimer(CreditsFallbackTimer);
	}

	if (URetrieveCreditsWidget* Credits = BoundCreditsWidget.Get())
	{
		Credits->OnCreditsCompleted.RemoveDynamic(this, &ARetrieveGameMode::HandleCreditsCompleted);
	}
	BoundCreditsWidget.Reset();
	bCreditsActive = false;
	PendingCreditsContinuation = nullptr;
	
	Super::EndPlay(EndPlayReason);
}

ARetrieveGameState* ARetrieveGameMode::GetRetrieveGameState() const
{
	return GetGameState<ARetrieveGameState>();
}

bool ARetrieveGameMode::IsRequestorHost(const APlayerController* Requestor) const
{
	if (!Requestor || !Requestor->PlayerState)
	{
		return false;
	}
	
	const ARetrieveGameState* GS = GetRetrieveGameState();
	return GS && GS->GetHostPlayerState() == Requestor->PlayerState;
}

void ARetrieveGameMode::BootstrapNewGameQuest()
{
	// 개발용 패스트 패스(bSkipMainMenuOnBoot): 리셋 후 Awakening을 동기적으로 발생, 타이밍 오프닝 없음.
	ResetNewGameState();

	if (ARetrieveGameState* GS = GetRetrieveGameState())
	{
		if (UQuestBranchComponent* Quest = GS->GetQuestBranchComponent())
		{
			Quest->CompleteStep(RetrieveGameplayTags::Quest_Step_Awakening);
		}
	}
}

void ARetrieveGameMode::HandleRevealGate(FGameplayTag Channel, const FRetrieveRevealGatePayload& Message)
{
	// 이번 새 게임에 대한 로딩 커버가 사라질 때 정확히 한 번만 오프닝을 시작
	if (Message.bBlocked || !bOpeningArmed)
	{
		return;
	}
	bOpeningArmed = false;
	StartOpeningSequence();
}

void ARetrieveGameMode::HandlePlayerDied(FGameplayTag Channel, const FPlayerDiedPayload& Message)
{
	/**
	 * TODO(coop): 게스트가 사망할 경우, 세션 전체를 Result로 전환하면 안 됨.
	 * Payload.DeadActor가 호스트 폰인지 필터링하여 게스트 사망은 로컬 사망 오버레이 + 로컬 리스폰으로 라우팅하고,
	 * 호스트 사망 시에는 세션을 해산할것 (평행 우주 모델)
	 */
	
	// 시네마틱 재생 중 사망(해저드/잔류 투사체 등)이면 재생을 먼저 정리한다 —
	// 시퀀서와 Result 화면이 카메라/UI를 두고 싸우지 않도록. 미재생 시 no-op.
	if (UWorld* World = GetWorld())
	{
		if (URetrieveCinematicSubsystem* Cinematic = World->GetSubsystem<URetrieveCinematicSubsystem>())
		{
			Cinematic->StopCinematic();
		}
	}

	if (ARetrieveGameState* GS = GetRetrieveGameState())
	{
		GS->TransitionTo(ERetrieveSessionState::Result);
	}
}

void ARetrieveGameMode::RespawnPlayerAtTransform(APlayerController* Requestor, const FTransform& RespawnTransform)
{
	APawn* Pawn = Requestor ? Requestor->GetPawn() : nullptr;
	if (ARetrieveAlsCombatCharacter* CombatPawn = Cast<ARetrieveAlsCombatCharacter>(Pawn))
	{
		CombatPawn->Revive(RespawnTransform);

		// 리스폰 지점 셀이 아직 로드되지 않아 땅으로 꺼지는 것을 막는다.
		// 빠른 이동과 동일한 스트리밍 대기(이동·충돌 잠금 → 임시 스트리밍 소스 → 지면 스냅)를 재사용.
		UGameInstance* GI = GetGameInstance();
		if (URetrieveSaveSubsystem* SaveSubsystem = GI ? GI->GetSubsystem<URetrieveSaveSubsystem>() : nullptr)
		{
			SaveSubsystem->BeginStreamedTeleport(Requestor, RespawnTransform, NAME_None, true);
		}
	}
	else if (Requestor)
	{
		// 폰이 파괴/유실된 비정상 경로(외부 Destroy, 과거 빌드의 KillZ 파괴 등) 최후 방어:
		// Revive 대상이 없으므로 기본 폰을 체크포인트에 새로 스폰해 빙의시킨다.
		// (정상 경로는 위의 Revive 재사용 — 인벤토리/체력 컴포넌트 상태 유지)
		UE_LOG(LogTemp, Warning,
			TEXT("[GameMode] RespawnPlayerAtTransform: 유효한 폰이 없어 RestartPlayerAtTransform 폴백으로 새 폰을 스폰합니다 (Requestor=%s)"),
			*GetNameSafe(Requestor));
		RestartPlayerAtTransform(Requestor, RespawnTransform);
	}
}

void ARetrieveGameMode::ResetWorldForNewGame()
{
	ARetrieveGameState* GS = GetRetrieveGameState();
	if (!GS)
	{
		return;
	}
	GS->ResetCheckpointForNewGame();

	if (UGameInstance* GI = GetGameInstance())
	{
		if (URetrieveSaveSubsystem* SaveSubsystem = GI->GetSubsystem<URetrieveSaveSubsystem>())
		{
			SaveSubsystem->ResetRescueEncountersForNewGame();
		}
	}

	for (TActorIterator<ARetrieveRescueEncounter> It(GetWorld()); It; ++It)
	{
		It->ResetForNewGame();
	}
	for (TActorIterator<ARetrieveLostCargoEncounter> It(GetWorld()); It; ++It)
	{
		It->ResetForNewGame();
	}
	for (TActorIterator<ARetrieveQuestEncounter> It(GetWorld()); It; ++It)
	{
		It->ResetForNewGame();
	}

	if (UQuestBranchComponent* Quest = GS->GetQuestBranchComponent())
	{
		Quest->ResetForTest();
	}
	
	if (ALumenCharacter* Lumen = FindLumen())
	{
		Lumen->SetRetired(false);
	}

	bHandoverCinematicPlayed = false;
	bEndgameSequenceStarted = false;
}

void ARetrieveGameMode::ResetNewGameState()
{
	ResetWorldForNewGame();

	if (UGameInstance* GI = GetGameInstance())
	{
		if (URetrieveSaveSubsystem* SaveSub =
			GI->GetSubsystem<URetrieveSaveSubsystem>())
		{
			SaveSub->ResetProgressForNewGame();
		}
	}
}

void ARetrieveGameMode::ArmOpeningSequence()
{
	bOpeningArmed = true;
	OpeningBeatIndex = 0;

	// 방금 리셋된(빈) 원장을 기준으로 알림 베이스라인을 재시딩하여, 첫 CompleteStep이 깨끗한 Locked->Active로 읽히도록 함
	// TODO(coop): 현재는 호스트 서브시스템에만 해당하며, 게스트는 리플리케이션으로부터 도출하도록 하기
	if (UWorld* World = GetWorld())
	{
		if (UQuestNotificationSubsystem* NotificationSubsystem = World->GetSubsystem<UQuestNotificationSubsystem>())
		{
			NotificationSubsystem->ResetBaseline();
		}
	}
}

void ARetrieveGameMode::StartOpeningSequence()
{
	// New Game 전용 경로(HandleNewGame -> Reveal)에서만 도달하므로 여기서 컷씬을 시작 (호스트 로컬) // TODO(coop)
	const bool bCinematicPlaying = TryPlayOpeningCinematic();

	if (!OpeningSequence || OpeningSequence->Beats.Num() == 0)
	{
		FallbackStartFirstQuest(); // 에셋 작성X -> 바로 첫 퀘스트 시작
		return;
	}

	// 컷씬 재생이 실제로 시작된 경우에만 종료 대기 게이팅 (실패/미설정 시 비트 즉시 시작 -> 영원 대기 방지)
	if (bCinematicPlaying && bWaitForIntroCinematic)
	{
		if (UWorld* World = GetWorld())
		{
			OpeningCinematicListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FRetrieveCinematicStatePayload>(
				RetrieveGameplayTags::Channel_Cinematic_Changed,[WeakThis = TWeakObjectPtr<ARetrieveGameMode>(this)]
				(FGameplayTag, const FRetrieveCinematicStatePayload& Message)
				{
					ARetrieveGameMode* GameMode = WeakThis.Get();
					if (GameMode && !Message.bActive) // 컷씬 종료됨
					{
						GameMode->OpeningCinematicListener.Unregister();
						GameMode->OpeningBeatIndex = 0;
						GameMode->ScheduleNextOpeningBeat();
					}
				});
		}
		return; // 비트는 컷씬이 끝날 때 시작됨
	}

	OpeningBeatIndex = 0;
	ScheduleNextOpeningBeat();
}

bool ARetrieveGameMode::TryPlayOpeningCinematic()
{
	if (OpeningCinematic.IsNull())
	{
		return false;
	}

	UWorld* World = GetWorld();
	URetrieveCinematicSubsystem* CinematicSubsystem = World ? World->GetSubsystem<URetrieveCinematicSubsystem>() : nullptr;
	if (!CinematicSubsystem)
	{
		return false;
	}

	return CinematicSubsystem->PlayCinematicSoft(OpeningCinematic, OpeningCinematicParams);
}

void ARetrieveGameMode::ScheduleNextOpeningBeat()
{
	if (!OpeningSequence || !OpeningSequence->Beats.IsValidIndex(OpeningBeatIndex))
	{
		return; // 타임라인 종료
	}
	const float Delay = FMath::Max(0.01f, OpeningSequence->Beats[OpeningBeatIndex].DelayBeforeSeconds);
	GetWorldTimerManager().SetTimer(OpeningBeatTimer, this, &ARetrieveGameMode::FireOpeningBeat, Delay, false);
}

void ARetrieveGameMode::FireOpeningBeat()
{
	UWorld* World = GetWorld();
	if (!World || !OpeningSequence || !OpeningSequence->Beats.IsValidIndex(OpeningBeatIndex))
	{
		return;
	}

	const FOpeningBeat& Beat = OpeningSequence->Beats[OpeningBeatIndex];
	switch (Beat.Kind)
	{
	case EOpeningBeatKind::SystemMessageById:
		if (USystemMessageSubsystem* SystemMessageSubsystem = World->GetSubsystem<USystemMessageSubsystem>())
		{
			SystemMessageSubsystem->RequestMessageById(Beat.SystemMessageRowName); // 호스트 로컬 // TODO(coop)
		}
		break;

	case EOpeningBeatKind::SystemMessageText:
		{
			FRetrieveSystemMessagePayload Message;
			Message.Text = Beat.MessageText;
			Message.Duration = Beat.MessageDuration;
			UGameplayMessageSubsystem::Get(World).BroadcastMessage(RetrieveGameplayTags::Channel_UI_SystemMessage,
			                                                       Message); // 호스트 로컬 // TODO(coop)
		}
		break;

	case EOpeningBeatKind::CompleteQuestStep:
		if (ARetrieveGameState* GS = GetRetrieveGameState())
		{
			if (UQuestBranchComponent* Quest = GS->GetQuestBranchComponent())
			{
				Quest->CompleteStep(Beat.QuestStepTag); // 호스트 권한; 토스트는 서브시스템이 도출
			}
		}
		break;
	}

	++OpeningBeatIndex;
	ScheduleNextOpeningBeat();
}

void ARetrieveGameMode::FallbackStartFirstQuest()
{
	if (ARetrieveGameState* GS = GetRetrieveGameState())
	{
		if (UQuestBranchComponent* Quest = GS->GetQuestBranchComponent())
		{
			Quest->CompleteStep(RetrieveGameplayTags::Quest_Step_Awakening);
		}
	}
}

void ARetrieveGameMode::DebugStartOpeningSequence()
{
	ArmOpeningSequence();
	bOpeningArmed = false;
	StartOpeningSequence();
}

void ARetrieveGameMode::HandleEndgameStepChanged(FGameplayTag /*Channel*/, const FRetrieveQuestStepPayload& Message)
{
	if (Message.StepTag != RetrieveGameplayTags::Quest_Step_TalkedToLumen_Castle || bHandoverCinematicPlayed)
	{
		return;
	}

	const ARetrieveGameState* GS = GetRetrieveGameState();
	if (!GS || GS->GetSessionState() != ERetrieveSessionState::InGame)
	{
		return;
	}
	bHandoverCinematicPlayed = true;
	PlayEndgameThen(LumenCoreHandoverCinematic, LumenCoreHandoverParams, TFunction<void()>());
}

void ARetrieveGameMode::HandleQueenDefeated(FGameplayTag /*Channel*/, const FMonsterDiedPayload& /*Message*/)
{
	if (bEndgameSequenceStarted)
	{
		return;
	}
	bEndgameSequenceStarted = true;

	if (ARetrieveGameState* GS = GetRetrieveGameState())
	{
		if (UQuestBranchComponent* Quest = GS->GetQuestBranchComponent())
		{
			Quest->CompleteStep(RetrieveGameplayTags::Quest_Step_QueenDefeated); // Stage 7 완료
		}
	}

	// 엔딩 컷씬(레벨 시퀀스) -> 크레딧(WBP) -> 메인메뉴
	PlayEndgameThen(EndingCinematic, EndingCinematicParams,
		[WeakThis = TWeakObjectPtr<ARetrieveGameMode>(this)]()
		{
			ARetrieveGameMode* GameMode = WeakThis.Get();
			if (!GameMode)
			{
				return;
			}
			GameMode->ShowCreditsThen(
				[WeakInner = TWeakObjectPtr<ARetrieveGameMode>(GameMode)]()
				{
					if (ARetrieveGameMode* Inner = WeakInner.Get())
					{
						Inner->FinishGame();
					}
				});
		});
}

void ARetrieveGameMode::PlayEndgameThen(const TSoftObjectPtr<ULevelSequence>& Sequence,
                                        const FRetrieveCinematicPlayParams& Params, TFunction<void()> Next)
{
	UWorld* World = GetWorld();
	URetrieveCinematicSubsystem* CinematicSubsystem = World ? World->GetSubsystem<URetrieveCinematicSubsystem>() : nullptr;

	// TODO(coop): 현재 재생은 호출한 머신 로컬(호스트)이다. 게스트 동시 재생은 CinematicState OnRep 확장 시.
	const bool bStarted = (CinematicSubsystem && !Sequence.IsNull())
		? CinematicSubsystem->PlayCinematicSoft(Sequence, Params)
		: false;

	if (!bStarted)
	{
		if (Next)
		{
			Next();
		}
		return;
	}

	if (EndgameCinematicListener.IsValid())
	{
		UGameplayMessageSubsystem::Get(World).UnregisterListener(EndgameCinematicListener);
	}

	EndgameCinematicListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FRetrieveCinematicStatePayload>(
		RetrieveGameplayTags::Channel_Cinematic_Changed,
		[WeakThis = TWeakObjectPtr<ARetrieveGameMode>(this), Next](FGameplayTag, const FRetrieveCinematicStatePayload& Message)
		{
			ARetrieveGameMode* GameMode = WeakThis.Get();
			if (!GameMode || Message.bActive)
			{
				return;
			}
			GameMode->EndgameCinematicListener.Unregister();

			if (!Next)
			{
				return;
			}
			
			if (UWorld* TickWorld = GameMode->GetWorld())
			{
				TickWorld->GetTimerManager().SetTimerForNextTick(
					FTimerDelegate::CreateWeakLambda(GameMode, [Next]() { Next(); }));
			}
			else
			{
				Next();
			}
		});
}

void ARetrieveGameMode::ShowCreditsThen(TFunction<void()> Next)
{
	UWorld* World = GetWorld();

	// TODO(coop): 호스트 로컬 UI. 게스트에게도 크레딧을 보여주려면 별도 전파가 필요하다.
	ARetrievePlayerController* PC = World ? World->GetFirstPlayerController<ARetrievePlayerController>() : nullptr;
	if (!PC)
	{
		if (Next)
		{
			Next();
		}
		return;
	}

	// 메인메뉴 크레딧 버튼과 같은 진입점
	PC->OpenCreditsPanel();

	URetrieveCreditsWidget* Credits = Cast<URetrieveCreditsWidget>(PC->GetActivePanel());
	if (!Credits)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GameMode] 크레딧 패널을 열지 못했습니다. 크레딧을 건너뛰고 엔딩을 마무리합니다."));
		if (Next)
		{
			Next();
		}
		return;
	}

	PendingCreditsContinuation = MoveTemp(Next);
	bCreditsActive = true;

	// 종료 통지: 끝까지 재생되거나 플레이어가 ESC로 스킵하면 위젯이 브로드캐스트.
	BoundCreditsWidget = Credits;
	Credits->OnCreditsCompleted.AddDynamic(this, &ARetrieveGameMode::HandleCreditsCompleted);

	// 안전망: 통지가 영영 오지 않는 오작성 대비.
	if (CreditsFallbackSeconds > 0.f)
	{
		World->GetTimerManager().SetTimer(CreditsFallbackTimer,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[GameMode] 크레딧 안전망 타이머 만료. OnCreditsCompleted 통지가 오지 않아 강제로 마무리합니다. ")
					TEXT("WBP_Credits에 CreditsScrollBox가 있는지, bLoop가 켜져 있지 않은지 확인하세요."));
				FinishCredits();
			}),
			CreditsFallbackSeconds, false);
	}
}

void ARetrieveGameMode::HandleCreditsCompleted(bool /*bWasSkipped*/)
{
	FinishCredits();
}

void ARetrieveGameMode::FinishCredits()
{
	if (!bCreditsActive)
	{
		return;
	}
	bCreditsActive = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CreditsFallbackTimer);
	}

	if (URetrieveCreditsWidget* Credits = BoundCreditsWidget.Get())
	{
		Credits->OnCreditsCompleted.RemoveDynamic(this, &ARetrieveGameMode::HandleCreditsCompleted);
	}
	BoundCreditsWidget.Reset();
	// 위젯이 CompleteCredits 직후 스스로 RequestClose를 호출

	TFunction<void()> Next = MoveTemp(PendingCreditsContinuation);
	PendingCreditsContinuation = nullptr;
	if (!Next)
	{
		return;
	}

	if (UWorld* TickWorld = GetWorld())
	{
		TickWorld->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateWeakLambda(this, [Next]() { Next(); }));
	}
	else
	{
		Next();
	}
}

void ARetrieveGameMode::FinishGame()
{
	ARetrieveGameState* GS = GetRetrieveGameState();
	if (!GS)
	{
		return;
	}

	if (UQuestBranchComponent* Quest = GS->GetQuestBranchComponent())
	{
		Quest->CompleteStep(RetrieveGameplayTags::Quest_Step_GameComplete);
	}

	// 승리는 Result(사망 화면)를 거치지 않음.
	GS->TransitionTo(ERetrieveSessionState::MainMenu);
}

ALumenCharacter* ARetrieveGameMode::FindLumen() const
{
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ALumenCharacter> It(World); It; ++It)
		{
			return *It;
		}
	}
	return nullptr;
}
