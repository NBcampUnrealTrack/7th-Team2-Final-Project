#include "Core/RetrieveGameMode.h"

#include "RetrieveGameState.h"
#include "Player/RetrievePlayerController.h"
#include "Player/RetrievePlayerState.h"

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

void ARetrieveGameMode::OnWorldReadyForGameplay()
{
	if (ARetrieveGameState* GS = GetRetrieveGameState())
	{
		GS->TransitionTo(ERetrieveSessionState::MainMenu);
		
		// TODO: W_MainMenu 위젯 생성 이후, 메인메뉴에서 인게임 진입하도록 수정하기
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
	
	// TODO: GameState에 LastCheckpoint 필드가 생기면 체크포인트에서 재시작
	
	if (ARetrieveGameState* GS = GetRetrieveGameState())
	{
		GS->TransitionTo(ERetrieveSessionState::InGame);
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
