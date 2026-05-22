#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RetrieveGameMode.generated.h"

class ARetrieveGameState;

UCLASS()
class RETRIEVE_API ARetrieveGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARetrieveGameMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void PostLogin(APlayerController* NewPlayerController) override;

	/** 서버 전용. 요청자가 호스트가 아니면 거부됩니다. */
	void HandleNewGame(APlayerController* Requestor);
	void HandleRetry(APlayerController* Requestor);
	void HandleQuitToMenu(APlayerController* Requestor);

protected:
	/** 최초 PostLogin에서 한 번 발동. 이후 월드 파티션 스트리밍 준비 완료 델리게이트로 교체 예정. */
	void OnWorldReadyForGameplay();

	ARetrieveGameState* GetRetrieveGameState() const;

	bool IsRequestorHost(const APlayerController* Requestor) const;
};
