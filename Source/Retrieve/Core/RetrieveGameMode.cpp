#include "Core/RetrieveGameMode.h"

#include "Diagnostics/RetrieveDiagLog.h"
#include "RetrieveGameState.h"
#include "Character/RetrieveAlsCombatCharacter.h"
#include "Components/Combat/RetrieveHealthComponent.h"
#include "Data/RetrieveOpeningSequenceAsset.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Messaging/GameplayMessages/RetrieveGameplayMessageTypes.h"
#include "Player/RetrievePlayerController.h"
#include "Player/RetrievePlayerState.h"
#include "Quest/QuestBranchComponent.h"
#include "Subsystems/QuestNotificationSubsystem.h"
#include "Subsystems/SystemMessageSubsystem.h"
#include "UObject/UObjectGlobals.h"

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

	if (bSkipMainMenuOnBoot)
	{
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


	// TODO: 필드 리셋 훅(봉인 게이트, 가디언 코어) 추가
	ResetWorldForNewGame();
	ArmOpeningSequence(); // Awakening은 Reveal 이후 타이밍이 조절된 비트로 발생함 (StartOpeningSequence 참고)

	if (ARetrieveGameState* GS = GetRetrieveGameState())
	{
		GS->TransitionTo(ERetrieveSessionState::InGame);
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

	const FTransform RespawnTransform = GS->GetLastCheckpointOrFallback();
	RespawnPlayerAtTransform(Requestor, RespawnTransform);

	GS->TransitionTo(ERetrieveSessionState::Result == GS->GetSessionState()
		                 ? ERetrieveSessionState::InGame
		                 : ERetrieveSessionState::InGame);
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
		World->GetTimerManager().ClearTimer(OpeningBeatTimer);
	}
	
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
	ResetWorldForNewGame();
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
	}
}

void ARetrieveGameMode::ResetWorldForNewGame()
{
	ARetrieveGameState* GS = GetRetrieveGameState();
	if (!GS)
	{
		return;
	}
	GS->SeedDefaultCheckpointIfUnset();
	if (UQuestBranchComponent* Quest = GS->GetQuestBranchComponent())
	{
		Quest->ResetForTest();
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
	if (!OpeningSequence || OpeningSequence->Beats.Num() == 0)
	{
		FallbackStartFirstQuest(); // 에셋 작성X -> 바로 첫 퀘스트 시작
		return;
	}

	if (bWaitForIntroCinematic)
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
