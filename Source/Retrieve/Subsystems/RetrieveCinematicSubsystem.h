#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RetrieveCinematicSubsystem.generated.h"

class ALevelSequenceActor;
class ULevelSequence;
class ULevelSequencePlayer;
class USoundMix;
class UUserWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRetrieveCinematicFinished);

/** 시네마틱 재생 옵션. 기본값은 컷씬 표준(이동/카메라 입력 차단 + 어빌리티 억제 태그 부여). */
USTRUCT(BlueprintType)
struct FRetrieveCinematicPlayParams
{
	GENERATED_BODY()

	/** 재생 중 이동 입력을 차단합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Cinematic")
	bool bDisableMovementInput = true;

	/** 재생 중 카메라(Look) 입력을 차단합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Cinematic")
	bool bDisableLookInput = true;

	/** 재생 중 로컬 플레이어 ASC에 State.Player.Cinematic 루즈 태그를 부여합니다(어빌리티/락온 억제). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Cinematic")
	bool bApplyCinematicTag = true;

	/** 로컬 플레이어 조작을 완전히 차단합니다: PC 입력 비활성(DisableInput) + 캐릭터 무브먼트 정지(DisableMovement).
	 * 종료 시 복원됩니다. 위의 Ignore 계열 차단만으로는 점프(ACharacter::Jump 직행)와
	 * ALS/CharacterMovement가 시퀀서와 트랜스폼을 두고 싸우는 떨림을 못 막습니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Cinematic")
	bool bBlockPlayerControl = true;

	/** Space/ESC 연타(확인 창 내 2회)로 이 시네마틱을 스킵할 수 있습니다.
	 * 스토리상 반드시 봐야 하는 컷씬은 끄세요. 스킵은 StopCinematic과 동일한 정리 경로를 탑니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Cinematic")
	bool bSkippable = true;

	/** 재생 중 BGM(Music 채널)을 페이드로 끄고 종료 시 복원합니다.
	 * 시퀀스 자체의 오디오 트랙도 Music 클래스로 분류돼 있으면 함께 눌리므로, 컷씬 전용 사운드는
	 * SFX/Voice 채널(또는 미분류)로 두어야 합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Cinematic")
	bool bSuppressMusic = true;

	/** 재생 중 환경음(Ambience 채널)을 페이드로 끄고 종료 시 복원합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Cinematic")
	bool bSuppressAmbience = true;

	/** 시네마틱 중에만 표시할 오버레이 위젯들(자막/스킵 안내 등). 재생 시 생성되어 뷰포트에 올라가고 종료 시 제거됩니다.
	 * HUD 일괄 숨김(Channel.Cinematic.Changed 기반)의 영향을 받지 않는 "예외 UI" 등록 지점입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Cinematic")
	TArray<TSubclassOf<UUserWidget>> OverlayWidgetClasses;
};

/**
 * 레벨 시퀀스 컷씬 재생의 단일 진입점.
 * 재생/종료 시 GameState.SetCinematicActive를 호출해 기존 억제 계층(퀘스트 토스트/시스템 메시지/바크/대화창/패널 차단)과
 * 자동 연동되고, 입력 차단과 State.Player.Cinematic 태그 부여까지 일관 처리합니다.
 * 사용: (BP) Get RetrieveCinematicSubsystem -> Play Cinematic / (C++) PlayCinematic 또는 PlayCinematicSoft.
 * 동시 재생은 1개만 허용합니다. 재생은 호출한 머신 로컬입니다. TODO(coop): 게스트 화면 재생은 CinematicState OnRep 기반 확장.
 */
UCLASS()
class RETRIEVE_API URetrieveCinematicSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 시퀀스를 재생합니다. 시퀀스가 없거나 이미 재생 중이면 false를 반환합니다. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Cinematic", meta = (AutoCreateRefTerm = "Params"))
	bool PlayCinematic(ULevelSequence* Sequence, const FRetrieveCinematicPlayParams& Params);

	/** 소프트 레퍼런스 버전. 동기 로드 후 재생합니다. */
	bool PlayCinematicSoft(const TSoftObjectPtr<ULevelSequence>& Sequence, const FRetrieveCinematicPlayParams& Params);

	/** 재생 중인 시네마틱을 즉시 중단합니다. 종료 처리(태그 해제/상태 복원/OnCinematicFinished)가 함께 수행됩니다. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Cinematic")
	void StopCinematic();

	UFUNCTION(BlueprintPure, Category = "Retrieve|Cinematic")
	bool IsCinematicPlaying() const { return ActivePlayer != nullptr; }

	/** 재생 중이고 스킵 허용(bSkippable) 컷씬인지. */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Cinematic")
	bool IsActiveCinematicSkippable() const { return IsCinematicPlaying() && ActiveParams.bSkippable; }

	/** 스킵 키(Space/ESC) 입력 1회 통지. 첫 입력은 확인 프롬프트를 띄우고(Channel.Cinematic.SkipPrompt),
	 * 확인 창 안의 두 번째 입력이 스킵을 확정한다. 스킵 불가/미재생이면 false(입력 미소비).
	 * PC의 InputKey(뷰포트 raw 경로 — DisableInput과 무관하게 동작)가 호출한다. */
	bool NotifySkipKeyPressed();

	/** 시네마틱 종료 시 브로드캐스트(자연 종료/중단 공통, 1회). */
	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Cinematic")
	FRetrieveCinematicFinished OnCinematicFinished;

	/** 해당 위젯이 이 서브시스템이 관리하는 시네마틱 오버레이(예외 UI)인지. HUD 일괄 숨김에서 제외 판별용. */
	bool IsCinematicOverlayWidget(const UUserWidget* Widget) const { return Widget && ActiveOverlayWidgets.Contains(Widget); }

	// ---- UWorldSubsystem ----
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

protected:
	/** OnFinished(자연 종료)와 OnStop(중단) 공용 핸들러. bFinishHandled로 1회만 수행. */
	UFUNCTION()
	void HandleSequenceFinished();

	void SetCinematicStateOnGameState(bool bActive);
	void ApplyCinematicTagToLocalPlayer(bool bApply);
	void ApplyPlayerControlBlock(bool bBlock);

	/** 시퀀스의 AnimationMode 키가 저FPS로 미평가된 채 종료돼도 로코모션 ABP로 복귀하도록 보정. */
	void RestoreLocalPawnAnimationMode();

	/** ActiveParams의 bSuppressMusic/bSuppressAmbience에 따라 해당 채널을 사운드 믹스로 페이드 뮤트/복원. */
	void ApplyAudioSuppression(bool bApply);

	void ShowOverlayWidgets();
	void RemoveOverlayWidgets();

	/** 재생 중에만 존재하는 오버레이 위젯 인스턴스들 (OverlayWidgetClasses에서 생성). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UUserWidget>> ActiveOverlayWidgets;

	UPROPERTY(Transient)
	TObjectPtr<ULevelSequencePlayer> ActivePlayer;

	/** 조작 차단을 건 대상. 재생 중 폰 교체(사망 등)에 대비해 약참조로 보관. */
	TWeakObjectPtr<APlayerController> BlockedPlayerController;
	TWeakObjectPtr<APawn> BlockedPawn;

	/** CreateLevelSequencePlayer가 스폰한 임시 액터. 종료 후 재사용하지 않습니다. */
	UPROPERTY(Transient)
	TObjectPtr<ALevelSequenceActor> ActivePlayerActor;

	/** 음악/환경음 억제용 사운드 믹스(런타임 생성). 설정 볼륨 믹스와 별개로 곱연산 적용된다. */
	UPROPERTY(Transient)
	TObjectPtr<USoundMix> AudioSuppressMix;

	/** 스킵 확인 프롬프트 표시/숨김 브로드캐스트 (Channel.Cinematic.SkipPrompt). */
	void SetSkipPromptVisible(bool bVisible);

	FRetrieveCinematicPlayParams ActiveParams;
	bool bFinishHandled = false;
	bool bAudioSuppressed = false;

	/** 마지막 스킵 키 입력 시각(RealTimeSeconds). 음수 = 확인 대기 아님. */
	double LastSkipPressRealTime = -1.0;
	FTimerHandle SkipPromptResetTimer;
};
