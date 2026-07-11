#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Subsystems/SystemMessageSubsystem.h"
#include "RetrieveSystemMessageWidget.generated.h"

class UTextBlock;
class USoundBase;
struct FRetrieveCinematicStatePayload;
struct FRetrieveRevealGatePayload;
struct FRetrieveDialogueChangedPayload;

/**
 * 우상단 텍스트 전용 시스템 메시지 위젯. 큐는 USystemMessageSubsystem이 갖고, 이 위젯은 꺼내 보여주기만 하는 얇은 뷰입니다.
 * 시네마틱일 때만 억제하며, 표시 중 시네마틱/위젯 파괴가 겹치면 현재 항목을 큐로 되돌립니다.
 */
UCLASS()
class RETRIEVE_API URetrieveSystemMessageWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void NativeOnFocusLost(const FFocusEvent& InFocusEvent) override;
	
	/** OnQueued 콜백, 새 항목이 오면 표시를 시도합니다. */
	void HandleQueued();

	void HandleCinematicChanged(FGameplayTag Channel, const FRetrieveCinematicStatePayload& Message);
	void HandleRevealGate(FGameplayTag Channel, const FRetrieveRevealGatePayload& Message);
	void HandleDialogueChanged(FGameplayTag Channel, const FRetrieveDialogueChangedPayload& Message);

	/** 억제 중이 아니고 표시 중 아니라면 큐에서 하나 꺼내 표시하고, 비었으면 숨깁니다. */
	void PumpNext();

	UFUNCTION()
	void HandleHoldExpired();

	UFUNCTION()
	void HandleHideFinished();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MessageText;
	
	/** "Press Enter" 안내. 해제 필수 메시지에서만 표시됩니다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> DismissPrompt;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> ShowAnim;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> HideAnim;

	/** Enter로 닫는 튜토리얼 메시지의 식별용 점멸 테두리입니다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> TutorialPulseBorder;

	/**
	 * 이 위젯은 상위 HUD의 인밸리데이션 캐싱에 걸려 RenderOpacity 변화가 화면에 반영되지 않는다.
	 * (opacity 애니메이션은 값만 바뀌고 그려지지 않음이 실측으로 확인됨.) 반면 RenderTransform은
	 * 매 프레임 적용되므로, 점멸은 UMG 애니메이션 대신 NativeTick에서 스케일 펄스로 구동한다.
	 * TutorialPulseAnim은 더 이상 사용하지 않으며 남겨둔 레거시 바인딩이다.
	 */
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> TutorialPulseAnim;

	/** 튜토리얼 메시지가 처음 열릴 때 한 번 재생되는 UI 사운드입니다. */
	UPROPERTY(EditDefaultsOnly, Category = "Tutorial Feedback")
	TSoftObjectPtr<USoundBase> TutorialOpenSound = TSoftObjectPtr<USoundBase>(
		FSoftObjectPath(TEXT("/Game/Retrieve/Audio/UI/V3/SFX_UI_PanelClose_V3.SFX_UI_PanelClose_V3")));

	UPROPERTY(EditDefaultsOnly, Category = "Tutorial Feedback", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TutorialOpenSoundVolume = 0.3f;

private:
	void SetModalInputBlock(bool bEngage);
	void FocusSelfNextTick();
	/**
	 * 배경 보더 표시와 점멸(스케일 펄스)을 분리해 제어한다.
	 * @param bShowBorder 배경 보더를 표시할지. 메시지가 떠 있는 동안 항상 true.
	 * @param bPulse      점멸시킬지. Enter 해제형 메시지에서만 true, 자동 넘김형은 false(정적 보더).
	 */
	void SetTutorialFeedbackActive(bool bShowBorder, bool bPulse);

	TWeakObjectPtr<USystemMessageSubsystem> Subsystem;

	/** 튜토리얼 점멸(스케일 펄스) 상태. NativeTick에서 TutorialPulseBorder를 맥동시킨다. */
	bool bTutorialPulseActive = false;
	float TutorialPulseElapsed = 0.f;

	/** 현재 표시 중인 항목. 시네마틱/파괴 시 큐로 되돌립니다. 위젯이 가진 유일한 메시지 상태. */
	FSystemMessageEntry CurrentEntry;
	bool bHasCurrent = false;
	bool bShowing = false;
	bool bCinematicActive = false;
	bool bRevealBlocked = false;
	bool bDialogueActive = false;
	bool bModalInputActive = false;
	
	FDelegateHandle QueuedHandle;
	FGameplayMessageListenerHandle CinematicHandle;
	FGameplayMessageListenerHandle RevealHandle;
	FGameplayMessageListenerHandle DialogueHandle;
	FTimerHandle HoldTimer;
};
