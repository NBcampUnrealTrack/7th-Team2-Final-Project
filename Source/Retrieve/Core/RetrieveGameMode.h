#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Subsystems/RetrieveCinematicSubsystem.h" // FRetrieveCinematicPlayParams (OpeningCinematicParams)
#include "RetrieveGameMode.generated.h"

class ALumenCharacter;
class ARetrieveGameState;
class ULevelSequence;
class URetrieveCreditsWidget;
class URetrieveOpeningSequenceAsset;
struct FMonsterDiedPayload;
struct FPlayerDiedPayload;
struct FRetrieveQuestStepPayload;
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

	/** 서버 전용. 가장 최근 저장 슬롯을 로드하고 InGame으로 전환합니다(메인메뉴 "이어하기"). */
	void HandleContinueGame(APlayerController* Requestor);

	/** 서버 전용. 지정 슬롯을 로드하고 InGame으로 전환합니다(메인메뉴 "불러오기" 슬롯 선택). */
	void HandleLoadGameSlot(APlayerController* Requestor, int32 SlotIndex);

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

	/** PIE 테스트에서 메뉴·진입 로딩·오프닝 시네마틱을 모두 건너뛰는 개발자 모드입니다. */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Dev")
	bool bDeveloperSkipIntroFlow = false;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Dev")
	/** 로딩 화면은 남기고 진행하는 개발자 모드입니다. */
	bool bSkipMainMenuOnBoot = false;
	
	FGameplayMessageListenerHandle PlayerDiedListener;

	/** 로딩 커버가 불투명해진 뒤 리스폰(Revive)을 실행하기 위한 지연 타이머. */
	FTimerHandle RespawnCoverDelayTimerHandle;

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

	/** OpeningCinematic이 설정되어 있으면 CinematicSubsystem으로 재생을 시작합니다. 재생이 시작되면 true. */
	bool TryPlayOpeningCinematic();

	/** 애셋이 작성되지 않은 경우에도 Awakening을 발생시켜 첫 퀘스트가 시작되게 합니다. */
	void FallbackStartFirstQuest();

public:
	/** 치트/디버그: 오프닝 타임라인을 즉시 재실행합니다. Reveal 대기를 생략합니다. */
	void DebugStartOpeningSequence();

protected:
	/** 순서가 있는 오프닝 비트. BP_RetrieveGameMode에서 할당. */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Opening")
	TObjectPtr<URetrieveOpeningSequenceAsset> OpeningSequence;

	/** New Game 오프닝 컷씬(레벨 시퀀스). 설정 시 로딩 커버가 걷힌 직후 재생됩니다. BP_RetrieveGameMode에서 할당. */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Opening")
	TSoftObjectPtr<ULevelSequence> OpeningCinematic;

	/** 오프닝 컷씬 재생 옵션. BGM/환경음 억제(bSuppressMusic/bSuppressAmbience), 입력 차단,
	 * 오버레이 위젯(자막/스킵 안내) 등을 BP_RetrieveGameMode에서 조정합니다. */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Opening")
	FRetrieveCinematicPlayParams OpeningCinematicParams;

	/** 인트로 컷씬이 끝날 때까지 메시지/퀘스트 비트를 게이팅합니다.
	 * OpeningCinematic 재생이 실제로 시작된 경우에만 적용됩니다(재생 실패 시 비트가 즉시 시작). */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Opening")
	bool bWaitForIntroCinematic = false;

	bool bOpeningArmed = false;
	int32 OpeningBeatIndex = 0;
	FTimerHandle OpeningBeatTimer;
	FGameplayMessageListenerHandle RevealGateListener;
	FGameplayMessageListenerHandle OpeningCinematicListener;
#pragma endregion

#pragma region Endgame (Stage 6/7)

	/**
	 * 엔딩 오케스트레이터. GameMode는 서버에만 존재하므로 호스트 권한이 자동으로 보장됩니다.
	 *   Stage 6 완료(코어 인계) → 인계 컷씬
	 *   여왕 처치 → Quest.Step.QueenDefeated → 엔딩 컷씬 → 크레딧 → GameComplete → 메인메뉴
	 * 컷씬 슬롯이 비어 있으면 그 단계를 건너뛰고 즉시 다음으로 넘어갑니다.
	 */
protected:
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Endgame")
	TSoftObjectPtr<ULevelSequence> LumenCoreHandoverCinematic;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Endgame")
	FRetrieveCinematicPlayParams LumenCoreHandoverParams;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Endgame")
	TSoftObjectPtr<ULevelSequence> EndingCinematic;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Endgame")
	FRetrieveCinematicPlayParams EndingCinematicParams;
	
	/**
	 * 크레딧 안전망(초). URetrieveCreditsWidget::OnCreditsCompleted를 받지 못해도 이 시간이 지나면 마무리합니다.
	 * 0 이하면 안전망 없음. 실제 크레딧 길이보다 조금 길게 잡으세요.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Endgame")
	float CreditsFallbackSeconds = 90.f;

	void HandleQueenDefeated(FGameplayTag Channel, const FMonsterDiedPayload& Message);
	void HandleEndgameStepChanged(FGameplayTag Channel, const FRetrieveQuestStepPayload& Message);

	void PlayEndgameThen(const TSoftObjectPtr<ULevelSequence>& Sequence, const FRetrieveCinematicPlayParams& Params, TFunction<void()> Next);

	/** 크레딧 패널을 열고 끝나면 Next를 실행합니다. 패널을 못 띄우면 Next를 즉시 실행합니다. */
	void ShowCreditsThen(TFunction<void()> Next);

	/** 크레딧 정리 + 대기 중인 후속 단계 실행. 통지/타이머 어느 쪽으로 들어와도 1회만 수행됩니다. */
	void FinishCredits();

	UFUNCTION()
	void HandleCreditsCompleted(bool bWasSkipped);

	/** 엔딩 체인의 끝. GameComplete 기록 후 메인메뉴로 전환(레벨 로드 없음). */
	void FinishGame();

	ALumenCharacter* FindLumen() const;

	FGameplayMessageListenerHandle QueenDefeatedListener;
	FGameplayMessageListenerHandle EndgameStepChangedListener;
	FGameplayMessageListenerHandle EndgameCinematicListener;

	/** OnCreditsCompleted를 바인딩한 패널. 해제용으로만 들고 있으며 수명은 PlayerController가 관리합니다.
	 *  TODO(coop): 크레딧은 호스트 화면에만 뜹니다. */
	TWeakObjectPtr<URetrieveCreditsWidget> BoundCreditsWidget;

	FTimerHandle CreditsFallbackTimer;

	/** 크레딧이 끝난 뒤 실행할 후속 단계(= FinishGame). */
	TFunction<void()> PendingCreditsContinuation;

	/** 크레딧 진행 중 방송/타이머 중복 진입 방지. 후속 단계 유무와 무관하게 판정합니다. */
	bool bCreditsActive = false;

	/** 코어 인계 컷씬 1회 재생 보장(세션 로컬). */
	bool bHandoverCinematicPlayed = false;

	/** 엔딩 체인 1회 진입 보장. */
	bool bEndgameSequenceStarted = false;
#pragma endregion
};
