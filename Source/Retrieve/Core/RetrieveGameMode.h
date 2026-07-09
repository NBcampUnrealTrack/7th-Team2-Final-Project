#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "RetrieveGameMode.generated.h"

class ARetrieveGameState;
class URetrieveOpeningSequenceAsset;
struct FPlayerDiedPayload;
struct FRetrieveRevealGatePayload;

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
	void HandleUnstuck(APlayerController* Requestor);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	/** 최초 PostLogin에서 한 번 발동. 이후 월드 파티션 스트리밍 준비 완료 델리게이트로 교체 예정. */
	void OnWorldReadyForGameplay();

	ARetrieveGameState* GetRetrieveGameState() const;

	bool IsRequestorHost(const APlayerController* Requestor) const;
	
	/** 호스트 전용. Channel_Player_Died 발생 시 세션을 Result 상태로 라우팅합니다. */
	void HandlePlayerDied(FGameplayTag Channel, const FPlayerDiedPayload& Message);
	
	/** 호스트 전용. 살아있는 폰을 RespawnTransform 위치에서 부활시킵니다*/
	void RespawnPlayerAtTransform(APlayerController* Requestor, const FTransform& RespawnTransform);

	/** 개발/테스트 편의 플래그. true이면 부팅 시 메인 메뉴를 건너뛰고 즉시 게임플레이를 시작합니다. 기본값 false = 메뉴 표시. */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Dev")
	bool bSkipMainMenuOnBoot = true;
	
	FGameplayMessageListenerHandle PlayerDiedListener;

#pragma region Opening Sequence (New Game)

protected:
	/** 새 게임을 위해 퀘스트 원장을 리셋하고 체크포인트를 시딩합니다. Awakening은 발생시키지 않습니다. */
	void ResetWorldForNewGame();
	
	/** 오프닝 준비: 알림 베이스라인을 리셋하고 Reveal 게이트 해제를 기다립니다. */
	void ArmOpeningSequence();

	void BootstrapNewGameQuest();
	void HandleRevealGate(FGameplayTag Channel, const FRetrieveRevealGatePayload& Message);
	void StartOpeningSequence();
	void ScheduleNextOpeningBeat();
	void FireOpeningBeat();

	/** 애셋이 작성되지 않은 경우에도 Awakening을 발생시켜 첫 퀘스트가 시작되게 합니다. */
	void FallbackStartFirstQuest();

public:
	/** 치트/디버그: 오프닝 타임라인을 즉시 재실행합니다. Reveal 대기를 생략합니다. */
	void DebugStartOpeningSequence();

protected:
	/** 순서가 있는 오프닝 비트. BP_RetrieveGameMode에서 할당. */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Opening")
	TObjectPtr<URetrieveOpeningSequenceAsset> OpeningSequence;

	/** 인트로 컷씬이 끝날 때까지 메시지/퀘스트 비트를 게이팅합니다.
	 * 인트로 Level Sequence가 생기기 전까지 false로 유지하세요. */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Opening")
	bool bWaitForIntroCinematic = false;

	bool bOpeningArmed = false;
	int32 OpeningBeatIndex = 0;
	FTimerHandle OpeningBeatTimer;
	FGameplayMessageListenerHandle RevealGateListener;
	FGameplayMessageListenerHandle OpeningCinematicListener;
#pragma endregion
};
