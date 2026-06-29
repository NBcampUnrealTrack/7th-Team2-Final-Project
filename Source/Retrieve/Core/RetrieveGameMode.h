#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "RetrieveGameMode.generated.h"

class ARetrieveGameState;
struct FPlayerDiedPayload;

UCLASS()
class RETRIEVE_API ARetrieveGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARetrieveGameMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void PostLogin(APlayerController* NewPlayerController) override;
	virtual void BeginPlay() override;

	/** 서버 전용. 요청자가 호스트가 아니면 거부됩니다. */
	void HandleNewGame(APlayerController* Requestor);
	void HandleRetry(APlayerController* Requestor);
	void HandleQuitToMenu(APlayerController* Requestor);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	/** 최초 PostLogin에서 한 번 발동. 이후 월드 파티션 스트리밍 준비 완료 델리게이트로 교체 예정. */
	void OnWorldReadyForGameplay();

	ARetrieveGameState* GetRetrieveGameState() const;

	bool IsRequestorHost(const APlayerController* Requestor) const;

	/** 호스트 전용. ResetForTest + Awakening 스텝을 완료합니다. */
	void BootstrapNewGameQuest();

	/** 호스트 전용. Channel_Player_Died 발생 시 세션을 Result 상태로 라우팅합니다. */
	void HandlePlayerDied(FGameplayTag Channel, const FPlayerDiedPayload& Message);
	
	/** 호스트 전용. 살아있는 폰을 RespawnTransform 위치에서 부활시킵니다*/
	void RespawnPlayerAtTransform(APlayerController* Requestor, const FTransform& RespawnTransform);

	/** 개발/테스트 편의 플래그. true이면 부팅 시 메인 메뉴를 건너뛰고 즉시 게임플레이를 시작합니다. 기본값 false = 메뉴 표시. */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Dev")
	bool bSkipMainMenuOnBoot = true;
	
	FGameplayMessageListenerHandle PlayerDiedListener;
};
