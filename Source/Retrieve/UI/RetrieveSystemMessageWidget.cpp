#include "RetrieveSystemMessageWidget.h"

#include "Components/TextBlock.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "GameplayTags/RetrieveGameplayTags.h"
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

void URetrieveSystemMessageWidget::PumpNext()
{
	if (bShowing || bCinematicActive || bRevealBlocked)
	{
		return; // 표시 중이거나 억제 중: 현재 상태 유지
	}

	USystemMessageSubsystem* SystemMessageSubsystem = Subsystem.Get();
	FSystemMessageEntry NextEntry;
	
	if (!SystemMessageSubsystem || !SystemMessageSubsystem->DequeueNext(NextEntry))
	{
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
			Self->SetKeyboardFocus();
		}
	});
}
