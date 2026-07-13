#include "RetrieveSystemMessageWidget.h"

#include "Components/TextBlock.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "Player/RetrievePlayerController.h"

void URetrieveSystemMessageWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	SetVisibility(ESlateVisibility::Collapsed);
	if (HideAnim)
	{
		FWidgetAnimationDynamicEvent FinishedEvent;
		FinishedEvent.BindDynamic(this, &URetrieveSystemMessageWidget::HandleHideFinished);
		BindToAnimationFinished(HideAnim, FinishedEvent);
	}

	if (UWorld* World = GetWorld())
	{
		if (USystemMessageSubsystem* SystemMessageSubsystem = World->GetSubsystem<USystemMessageSubsystem>())
		{
			Subsystem = SystemMessageSubsystem;
			QueuedHandle = SystemMessageSubsystem->OnQueued().AddUObject(
				this, &URetrieveSystemMessageWidget::HandleQueued);
		}
		// 시네마틱 억제 신호만 구독, Channel.UI.SystemMessage는 구독하지 않음 (모든 입력은 서브시스템으로)
		CinematicHandle = UGameplayMessageSubsystem::Get(World).RegisterListener<FRetrieveCinematicStatePayload>(
			RetrieveGameplayTags::Channel_Cinematic_Changed, this,
			&URetrieveSystemMessageWidget::HandleCinematicChanged);

		RevealHandle = UGameplayMessageSubsystem::Get(World).RegisterListener<FRetrieveRevealGatePayload>(
			RetrieveGameplayTags::Channel_UI_RevealGate, this,
			&URetrieveSystemMessageWidget::HandleRevealGate);

		// 대화 중에는 HUD 전체가 접히므로(UpdateHUDNarrativeVisibility) 위젯이 포커스/모달 입력을
		// 잃는다. 시네마틱과 동일하게 억제해 현재 항목을 큐로 되돌리고, 대화 종료 후 처음부터
		// 다시 표시하며 포커스·모달 입력을 재수립한다.
		DialogueHandle = UGameplayMessageSubsystem::Get(World).RegisterListener<FRetrieveDialogueChangedPayload>(
			RetrieveGameplayTags::Channel_UI_DialogueChanged, this,
			&URetrieveSystemMessageWidget::HandleDialogueChanged);
	}

	// 위젯이 막 생성됨: 그동안 큐에 쌓여 있던 항목을 바로 표시하기 시작
	PumpNext();
}

void URetrieveSystemMessageWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		UGameplayMessageSubsystem::Get(World).UnregisterListener(CinematicHandle);
		UGameplayMessageSubsystem::Get(World).UnregisterListener(RevealHandle);
		UGameplayMessageSubsystem::Get(World).UnregisterListener(DialogueHandle);
		World->GetTimerManager().ClearTimer(HoldTimer);
	}
	if (USystemMessageSubsystem* SystemMessageSubsystem = Subsystem.Get())
	{
		SystemMessageSubsystem->OnQueued().Remove(QueuedHandle);
		// HUD가 표시 도중 파괴되는 경우(리스폰/세션 전환): 현재 항목을 큐로 되돌려 새 위젯이 이어서 표시.
		if (bHasCurrent)
		{
			SystemMessageSubsystem->RequeueFront(CurrentEntry);
			bHasCurrent = false;
		}
	}
	Super::NativeDestruct();
}

FReply URetrieveSystemMessageWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// 해제 필수 메시지를 보여주는 동안에만 Enter를 소비해 다음으로 넘김.
	if (bShowing && bHasCurrent && CurrentEntry.bRequiresDismiss)
	{
		if (InKeyEvent.GetKey() == EKeys::Enter)
		{
			HandleHoldExpired(); // 타이머가 만료됐을 때와 동일한 경로: 페이드 → PumpNext → 다음 항목
			return FReply::Handled();
		}
	}
	
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void URetrieveSystemMessageWidget::NativeOnFocusLost(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnFocusLost(InFocusEvent);
	if (bShowing && bHasCurrent && CurrentEntry.bRequiresDismiss)
	{
		FocusSelfNextTick();
	}
}

void URetrieveSystemMessageWidget::HandleQueued()
{
	PumpNext();
}

void URetrieveSystemMessageWidget::HandleCinematicChanged(FGameplayTag Channel,
                                                          const FRetrieveCinematicStatePayload& Message)
{
	bCinematicActive = Message.bActive;
	if (bCinematicActive)
	{
		// 억제 시작: 홀드 타이머 정지, 현재 항목을 큐로 되돌리고 숨김
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(HoldTimer);
		}
		StopAllAnimations();
		if (bHasCurrent)
		{
			if (USystemMessageSubsystem* SystemMessageSubsystem = Subsystem.Get())
			{
				SystemMessageSubsystem->RequeueFront(CurrentEntry); // 시네마틱 종료 후 처음부터 다시 표시.
			}
			bHasCurrent = false;
		}
		bShowing = false;
		SetModalInputBlock(false);
		SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		PumpNext(); // 억제 해제: 되돌린 항목부터 재개.
	}
}

void URetrieveSystemMessageWidget::HandleRevealGate(FGameplayTag Channel, const FRetrieveRevealGatePayload& Message)
{
	bRevealBlocked = Message.bBlocked;
	if (bRevealBlocked)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(HoldTimer);
		}
		StopAllAnimations();
		if (bHasCurrent)
		{
			if (USystemMessageSubsystem* SystemMessageSubsystem = Subsystem.Get())
			{
				SystemMessageSubsystem->RequeueFront(CurrentEntry);
			}
			bHasCurrent = false;
		}
		bShowing = false;
		SetModalInputBlock(false);
		SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		PumpNext();
	}
}

void URetrieveSystemMessageWidget::HandleDialogueChanged(FGameplayTag Channel,
                                                         const FRetrieveDialogueChangedPayload& Message)
{
	bDialogueActive = Message.bActive;
	if (bDialogueActive)
	{
		// 억제 시작: 시네마틱과 동일 경로. 홀드 타이머 정지, 현재 항목을 큐로 되돌리고 숨김.
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(HoldTimer);
		}
		StopAllAnimations();
		if (bHasCurrent)
		{
			if (USystemMessageSubsystem* SystemMessageSubsystem = Subsystem.Get())
			{
				SystemMessageSubsystem->RequeueFront(CurrentEntry); // 대화 종료 후 처음부터 다시 표시.
			}
			bHasCurrent = false;
		}
		bShowing = false;
		SetModalInputBlock(false);
		SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		PumpNext(); // 억제 해제: 되돌린 항목부터 재개(포커스·모달 입력 재수립).
	}
}

void URetrieveSystemMessageWidget::PumpNext()
{
	if (bShowing || bCinematicActive || bRevealBlocked || bDialogueActive)
	{
		return; // 표시 중이거나 억제 중: 현재 상태 유지
	}

	USystemMessageSubsystem* SystemMessageSubsystem = Subsystem.Get();
	FSystemMessageEntry NextEntry;
	
	if (!SystemMessageSubsystem || !SystemMessageSubsystem->DequeueNext(NextEntry))
	{
		SetTutorialFeedbackActive(false, false); // 큐 비움: 보더도 숨김
		SetModalInputBlock(false); // 큐가 비면 잡고 있던 입력 잠금 해제
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	CurrentEntry = NextEntry;
	bHasCurrent = true;
	bShowing = true;

	if (MessageText)
	{
		MessageText->SetRenderOpacity(1.f);
		MessageText->SetText(CurrentEntry.Text);
	}
	SetRenderOpacity(1.f);
	if (ShowAnim)
	{
		PlayAnimation(ShowAnim);
	}

	if (CurrentEntry.bRequiresDismiss)
	{
		SetVisibility(ESlateVisibility::Visible); // 포커스/키 입력을 받으려면 Visible
		SetTutorialFeedbackActive(true, true); // 해제형: 보더 + 점멸
		if (USoundBase* OpenSound = TutorialOpenSound.LoadSynchronous())
		{
			UGameplayStatics::PlaySound2D(this, OpenSound, TutorialOpenSoundVolume);
		}
		SetModalInputBlock(true);
		SetKeyboardFocus();
		if (DismissPrompt)
		{
			DismissPrompt->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		// HoldTimer 시작 안 함, Enter까지 대기
	}
	else
	{
		SetTutorialFeedbackActive(true, false); // 자동 넘김형: 보더는 정적으로 표시, 점멸 없음
		SetVisibility(ESlateVisibility::HitTestInvisible);
		SetModalInputBlock(false);
		if (DismissPrompt)
		{
			DismissPrompt->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(HoldTimer, this, &URetrieveSystemMessageWidget::HandleHoldExpired,
			                                  FMath::Max(0.5f, CurrentEntry.Duration), false);
		}
	}
}

void URetrieveSystemMessageWidget::HandleHoldExpired()
{
	// 현재 항목은 전체 표시 시간을 채웠으므로 소비 완료, 페이드아웃이 끝난 뒤 다음 항목으로 넘어감
	SetTutorialFeedbackActive(false, false);
	bHasCurrent = false;
	if (HideAnim)
	{
		PlayAnimation(HideAnim);
	}
	else
	{
		HandleHideFinished();
	}
}

void URetrieveSystemMessageWidget::HandleHideFinished()
{
	bShowing = false;
	SetVisibility(ESlateVisibility::Collapsed);
	PumpNext();
}

void URetrieveSystemMessageWidget::SetTutorialFeedbackActive(bool bShowBorder, bool bPulse)
{
	// 배경 보더는 메시지가 떠 있는 동안 항상 표시하고, 점멸(스케일 펄스)은 Enter 해제형에서만 켠다.
	// 점멸은 RenderTransform 스케일 펄스로 구동한다(NativeTick). 이 위젯은 상위 HUD 캐싱에
	// 걸려 RenderOpacity 변화가 화면에 반영되지 않으므로 opacity 애니메이션은 쓰지 않는다.
	bTutorialPulseActive = bShowBorder && bPulse;
	TutorialPulseElapsed = 0.f;

	if (TutorialPulseBorder)
	{
		TutorialPulseBorder->SetVisibility(
			bShowBorder ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		// 점멸을 끈 경우 스케일을 1로 고정해 정적 보더로 표시한다.
		TutorialPulseBorder->SetRenderScale(FVector2D(1.f, 1.f));
		TutorialPulseBorder->SetRenderOpacity(1.f);
	}
}

void URetrieveSystemMessageWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bTutorialPulseActive && TutorialPulseBorder)
	{
		// 1.0 → 1+Amplitude → 1.0 을 Period 주기로 부드럽게 반복(중심 피벗 기준 맥동).
		constexpr float Period = 0.75f;    // 한 맥동 주기(초)
		constexpr float Amplitude = 0.06f; // 최대 확대 비율(6%)
		TutorialPulseElapsed += InDeltaTime;
		const float Phase = (TutorialPulseElapsed / Period) * 2.f * PI;
		const float Scale = 1.f + Amplitude * 0.5f * (1.f - FMath::Cos(Phase));
		TutorialPulseBorder->SetRenderScale(FVector2D(Scale, Scale));
	}
}

void URetrieveSystemMessageWidget::SetModalInputBlock(bool bEngage)
{
	if (bEngage == bModalInputActive)
	{
		return;
	}
	bModalInputActive = bEngage;

	ARetrievePlayerController* PC = Cast<ARetrievePlayerController>(GetOwningPlayer());
	if (!PC)
	{
		return;
	}
	if (bEngage)
	{
		PC->EnterModalMessageInput(this);
	}
	else
	{
		PC->ExitModalMessageInput();
	}
}

void URetrieveSystemMessageWidget::FocusSelfNextTick()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	TWeakObjectPtr<URetrieveSystemMessageWidget> WeakThis(this);
	World->GetTimerManager().SetTimerForNextTick([WeakThis]()
	{
		URetrieveSystemMessageWidget* Self = WeakThis.Get();
		if (Self && Self->bShowing && Self->bHasCurrent && Self->CurrentEntry.bRequiresDismiss)
		{
			// 세션 상태 전환(UpdateInputMode)이 모달 잠금을 해제하고 GameOnly로 바꾸면
			// 포커스만 되찾아서는 Enter가 위젯에 오지 않아 메시지를 닫을 수 없다.
			// → 모달 입력 모드(UIOnly+위젯 포커스)를 다시 세운다.
			//   (위젯 쪽 bModalInputActive 플래그는 그대로 true이므로 SetModalInputBlock을
			//    거치지 않고 PC에 직접 재진입한다.)
			if (ARetrievePlayerController* PC = Cast<ARetrievePlayerController>(Self->GetOwningPlayer()))
			{
				PC->EnterModalMessageInput(Self);
			}
			Self->SetKeyboardFocus();
		}
	});
}
