#include "Core/RetrieveGameMode.h"

#include "RetrieveGameState.h"
#include "Character/RetrieveAlsCombatCharacter.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Messaging/GameplayMessages/RetrieveGameplayMessageTypes.h"
#include "Player/RetrievePlayerController.h"
#include "Player/RetrievePlayerState.h"
#include "Quest/QuestBranchComponent.h"

ARetrieveGameMode::ARetrieveGameMode(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PlayerControllerClass = ARetrievePlayerController::StaticClass();
	PlayerStateClass = ARetrievePlayerState::StaticClass();
	GameStateClass = ARetrieveGameState::StaticClass();
}

void ARetrieveGameMode::PostLogin(APlayerController* NewPlayerController)
{
	Super::PostLogin(NewPlayerController);
	
	ARetrieveGameState* GS = GetRetrieveGameState();
	if (!GS || !NewPlayerController)
	{
		return;
	}
	
	if (GS->GetHostPlayerState() == nullptr && NewPlayerController->PlayerState)
	{
		GS->SetHostPlayerState(NewPlayerController->PlayerState);
		OnWorldReadyForGameplay();
	}
}

void ARetrieveGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		PlayerDiedListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FPlayerDiedPayload>(
			RetrieveGameplayTags::Channel_Player_Died, this, &ARetrieveGameMode::HandlePlayerDied);
	}
}

void ARetrieveGameMode::OnWorldReadyForGameplay()
{
	ARetrieveGameState* GS = GetRetrieveGameState();
	if (!GS)
	{
		return;
	}
	
	GS->TransitionTo(ERetrieveSessionState::MainMenu);

	if (bSkipMainMenuOnBoot)
	{
		BootstrapNewGameQuest();
		GS->TransitionTo(ERetrieveSessionState::InGame);
	}
}

void ARetrieveGameMode::HandleNewGame(APlayerController* Requestor)
{
	if (!IsRequestorHost(Requestor))
	{
		return;
	}
	
	// TODO: 퀘스트, 봉인 게이트, 가디언 코어 등 필드 리셋 훅 추가
	BootstrapNewGameQuest();
	
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

void ARetrieveGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (PlayerDiedListener.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			UGameplayMessageSubsystem::Get(World).UnregisterListener(PlayerDiedListener);
		}
		PlayerDiedListener = FGameplayMessageListenerHandle();
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
	ARetrieveGameState* GS = GetRetrieveGameState();
	if (!GS)
	{
		return;
	}
	
	GS->SeedDefaultCheckpointIfUnset();
	
	if (UQuestBranchComponent* Quest = GS->GetQuestBranchComponent())
	{
		Quest->ResetForTest();
		Quest->CompleteStep(RetrieveGameplayTags::Quest_Step_Awakening);
	}
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
