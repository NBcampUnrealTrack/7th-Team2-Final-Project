#include "RetrieveQuestNotificationWidget.h"

#include "Components/RetainerBox.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameplayTags/RetrieveGameplayTags.h"

#define LOCTEXT_NAMESPACE "Retrieve.QuestNotification"

void URetrieveQuestNotificationWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetVisibility(ESlateVisibility::Collapsed);
	RefreshRevealMaterial();
	ResetReveal(false);

	if (HideAnim)
	{
		FWidgetAnimationDynamicEvent FinishedEvent;
		FinishedEvent.BindDynamic(this, &URetrieveQuestNotificationWidget::HandleHideFinished);
		BindToAnimationFinished(HideAnim, FinishedEvent);
	}

	if (UWorld* World = GetWorld())
	{
		if (UQuestNotificationSubsystem* NotificationSubsystem = World->GetSubsystem<UQuestNotificationSubsystem>())
		{
			Subsystem = NotificationSubsystem;
			QueuedHandle = NotificationSubsystem->OnQueued().AddUObject(this, &URetrieveQuestNotificationWidget::HandleQueued);
		}

		UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(World);
		CinematicHandle = MessageSubsystem.RegisterListener<FRetrieveCinematicStatePayload>(
			RetrieveGameplayTags::Channel_Cinematic_Changed, this,
			&URetrieveQuestNotificationWidget::HandleCinematicChanged);
		RevealHandle = MessageSubsystem.RegisterListener<FRetrieveRevealGatePayload>(
			RetrieveGameplayTags::Channel_UI_RevealGate, this, &URetrieveQuestNotificationWidget::HandleRevealGate);
	}

	// HUD 재구성 시 복구
	PumpNext();
}

void URetrieveQuestNotificationWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(World);
		MessageSubsystem.UnregisterListener(CinematicHandle);
		MessageSubsystem.UnregisterListener(RevealHandle);
		World->GetTimerManager().ClearTimer(HoldTimer);
	}
	if (UQuestNotificationSubsystem* NotificationSubsystem = Subsystem.Get())
	{
		NotificationSubsystem->OnQueued().Remove(QueuedHandle);
		if (bHasCurrent) // 토스트 도중 HUD가 파괴됨 -> 다음 위젯이 이어받도록 되돌리기
		{
			NotificationSubsystem->RequeueFront(CurrentEntry);
			bHasCurrent = false;
		}
	}
	RevealMaterial = nullptr;
	Super::NativeDestruct();
}

void URetrieveQuestNotificationWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	TickReveal(InDeltaTime);
}

void URetrieveQuestNotificationWidget::HandleQueued()
{
	PumpNext();
}

void URetrieveQuestNotificationWidget::HandleCinematicChanged(FGameplayTag Channel,
                                                              const FRetrieveCinematicStatePayload& Message)
{
	bCinematicActive = Message.bActive;
	if (bCinematicActive)
	{
		SuppressAndRequeue();
	}
	else
	{
		PumpNext();
	}
}

void URetrieveQuestNotificationWidget::HandleRevealGate(FGameplayTag Channel, const FRetrieveRevealGatePayload& Message)
{
	bRevealBlocked = Message.bBlocked;
	if (bRevealBlocked)
	{
		SuppressAndRequeue();
	}
	else
	{
		PumpNext();
	}
}

void URetrieveQuestNotificationWidget::SuppressAndRequeue()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HoldTimer);
	}
	StopAllAnimations();
	if (bHasCurrent)
	{
		if (UQuestNotificationSubsystem* NotificationSubsystem = Subsystem.Get())
		{
			NotificationSubsystem->RequeueFront(CurrentEntry); // 억제가 풀리면 이 항목부터 재개
		}
		bHasCurrent = false;
	}
	bShowing = false;
	ResetReveal(false);
	SetVisibility(ESlateVisibility::Collapsed);
}

void URetrieveQuestNotificationWidget::PumpNext()
{
	if (bShowing || bCinematicActive || bRevealBlocked)
	{
		return;
	}

	UQuestNotificationSubsystem* NotificationSubsystem = Subsystem.Get();
	FQuestNotificationEntry Next;
	if (!NotificationSubsystem || !NotificationSubsystem->DequeueNext(Next))
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	CurrentEntry = Next;
	bHasCurrent = true;
	bShowing = true;

	if (HeaderText)
	{
		HeaderText->SetText(ResolveHeaderText(Next.Kind));
	}
	if (QuestNameText)
	{
		QuestNameText->SetText(Next.QuestName);
	}

	SetRenderOpacity(1.f);
	SetVisibility(ESlateVisibility::HitTestInvisible);
	StartRevealIn();

	OnPresent(Next.Kind, Next.QuestName); // 페이드 인 직전 BP 훅

	if (ShowAnim)
	{
		PlayAnimation(ShowAnim);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(HoldTimer, this, &URetrieveQuestNotificationWidget::HandleHoldExpired,
		                                  FMath::Max(0.5f, Next.Duration), false);
	}
}

void URetrieveQuestNotificationWidget::HandleHoldExpired()
{
	bHasCurrent = false;
	StartRevealOut();
	if (HideAnim)
	{
		PlayAnimation(HideAnim);
	}
	else
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(HoldTimer, this, &URetrieveQuestNotificationWidget::HandleHideFinished,
			                                  FMath::Max(0.01f, RevealOutSeconds), false);
		}
		else
		{
			HandleHideFinished();
		}
	}
}

void URetrieveQuestNotificationWidget::HandleHideFinished()
{
	bShowing = false;
	ResetReveal(false);
	SetVisibility(ESlateVisibility::Collapsed);
	PumpNext();
}

FText URetrieveQuestNotificationWidget::ResolveHeaderText(EQuestNotificationKind Kind) const
{
	if (Kind == EQuestNotificationKind::Completed)
	{
		return CompletedHeader.IsEmpty() ? LOCTEXT("QuestCompleted", "QUEST COMPLETED") : CompletedHeader;
	}
	return StartedHeader.IsEmpty() ? LOCTEXT("QuestStarted", "QUEST STARTED") : StartedHeader;
}

void URetrieveQuestNotificationWidget::RefreshRevealMaterial()
{
	RevealMaterial = RevealRetainer ? RevealRetainer->GetEffectMaterial() : nullptr;
}

void URetrieveQuestNotificationWidget::ResetReveal(bool bVisible)
{
	RevealPhase = EQuestRevealPhase::Idle;
	RevealElapsed = 0.f;
	SetRevealSweep(bVisible ? VisibleSweep : HiddenSweep);
}

void URetrieveQuestNotificationWidget::StartRevealIn()
{
	RefreshRevealMaterial();
	RevealPhase = EQuestRevealPhase::Revealing;
	RevealElapsed = 0.f;
	if (RevealMaterial && !RevealDirectionParameter.IsNone())
	{
		RevealMaterial->SetScalarParameterValue(RevealDirectionParameter, 1.f);
	}
	SetRevealSweep(HiddenSweep);
}

void URetrieveQuestNotificationWidget::StartRevealOut()
{
	RefreshRevealMaterial();
	RevealPhase = EQuestRevealPhase::Hiding;
	RevealElapsed = 0.f;
	if (RevealMaterial && !RevealDirectionParameter.IsNone())
	{
		RevealMaterial->SetScalarParameterValue(RevealDirectionParameter, bUseDirectionForHide ? -1.f : 1.f);
	}
	SetRevealSweep(bUseDirectionForHide ? HiddenSweep : VisibleSweep);
}

void URetrieveQuestNotificationWidget::TickReveal(float DeltaTime)
{
	if (RevealPhase == EQuestRevealPhase::Idle)
	{
		return;
	}

	if (!RevealMaterial)
	{
		RefreshRevealMaterial();
	}
	if (!RevealMaterial)
	{
		return;
	}

	const float Duration = RevealPhase == EQuestRevealPhase::Revealing
		? FMath::Max(0.01f, RevealInSeconds)
		: FMath::Max(0.01f, RevealOutSeconds);

	RevealElapsed += FMath::Max(0.f, DeltaTime);
	const float Alpha = FMath::Clamp(RevealElapsed / Duration, 0.f, 1.f);
	const float SmoothAlpha = FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f);

	if (RevealPhase == EQuestRevealPhase::Revealing)
	{
		SetRevealSweep(FMath::Lerp(HiddenSweep, VisibleSweep, SmoothAlpha));
	}
	else
	{
		const float Start = bUseDirectionForHide ? HiddenSweep : VisibleSweep;
		const float End = bUseDirectionForHide ? VisibleSweep : HiddenSweep;
		SetRevealSweep(FMath::Lerp(Start, End, SmoothAlpha));
	}

	if (Alpha >= 1.f)
	{
		RevealPhase = EQuestRevealPhase::Idle;
	}
}

void URetrieveQuestNotificationWidget::SetRevealSweep(float Value)
{
	if (!RevealMaterial)
	{
		RefreshRevealMaterial();
	}
	if (RevealMaterial && !RevealSweepParameter.IsNone())
	{
		RevealMaterial->SetScalarParameterValue(RevealSweepParameter, Value);
	}
	if (RevealRetainer)
	{
		RevealRetainer->RequestRender();
		RevealRetainer->InvalidateLayoutAndVolatility();
	}
}
#undef LOCTEXT_NAMESPACE
