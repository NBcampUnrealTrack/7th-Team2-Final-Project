#include "UI/Menu/RetrieveCreditsWidget.h"

#include "Components/ScrollBox.h"

void URetrieveCreditsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ElapsedTime = 0.0f;
	EndHoldElapsed = 0.0f;
	bReachedEnd = false;
	bCompleted = false;
	bFastForwarding = false;

	if (CreditsScrollBox)
	{
		// 자동 재생 전용이므로 맨 위에서 시작한다(마우스 휠/스크롤바는 WBP 디자이너에서 비활성 권장).
		CreditsScrollBox->SetScrollOffset(0.0f);
	}
}

void URetrieveCreditsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	// 베이스(URetrieveUIVFXWidget)가 NativeTick에서 UI VFX를 처리하므로 반드시 먼저 호출한다.
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!CreditsScrollBox)
	{
		return;
	}

	ElapsedTime += InDeltaTime;
	if (ElapsedTime < StartDelay)
	{
		return;
	}

	// 끝에 도달한 뒤: 유지 시간이 지나면 반복하거나 마무리한다.
	if (bReachedEnd)
	{
		EndHoldElapsed += InDeltaTime;
		if (EndHoldElapsed < EndHoldTime)
		{
			return;
		}

		if (bLoop)
		{
			CreditsScrollBox->SetScrollOffset(0.0f);
			bReachedEnd = false;
			EndHoldElapsed = 0.0f;
			return;
		}

		CompleteCredits(/*bSkipped*/ false);
		if (bAutoCloseAtEnd)
		{
			RequestClose();
		}
		return;
	}

	const float EndOffset = CreditsScrollBox->GetScrollOffsetOfEnd();

	// 콘텐츠가 뷰포트보다 짧아 스크롤 여지가 없으면 곧바로 마무리 단계로 넘어간다.
	if (EndOffset <= KINDA_SMALL_NUMBER)
	{
		bReachedEnd = true;
		return;
	}

	// 빨리 감기 키를 누르고 있으면 배속으로 스크롤한다.
	const float EffectiveSpeed = ScrollSpeed *
		((bAllowFastForward && bFastForwarding) ? FastForwardMultiplier : 1.0f);

	const float NextOffset = CreditsScrollBox->GetScrollOffset() + EffectiveSpeed * InDeltaTime;
	if (NextOffset >= EndOffset)
	{
		CreditsScrollBox->SetScrollOffset(EndOffset);
		bReachedEnd = true;
	}
	else
	{
		CreditsScrollBox->SetScrollOffset(NextOffset);
	}
}

FReply URetrieveCreditsWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	// 빨리 감기: 지정 키를 누르고 있는 동안 가속(키 반복 이벤트여도 true 유지).
	if (bAllowFastForward && FastForwardKey.IsValid() && Key == FastForwardKey)
	{
		bFastForwarding = true;
		return FReply::Handled();
	}

	// 스킵: 허용 시 ESC/ToggleKey로 닫는다. 비허용 시 입력을 삼켜 아무 일도 일어나지 않게 한다
	// (베이스 URetrieveGamePanelWidget의 ESC-닫기를 우회한다).
	if (IsSkipKey(Key))
	{
		if (bAllowSkip)
		{
			CompleteCredits(/*bSkipped*/ true);
			RequestClose();
		}
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply URetrieveCreditsWidget::NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (FastForwardKey.IsValid() && InKeyEvent.GetKey() == FastForwardKey)
	{
		bFastForwarding = false;
		return FReply::Handled();
	}

	return Super::NativeOnKeyUp(InGeometry, InKeyEvent);
}

void URetrieveCreditsWidget::NativeOnFocusLost(const FFocusEvent& InFocusEvent)
{
	// 포커스를 잃으면 키 릴리즈 이벤트를 못 받아 빨리 감기가 고착될 수 있으므로 해제한다.
	bFastForwarding = false;
	Super::NativeOnFocusLost(InFocusEvent);
}

bool URetrieveCreditsWidget::IsSkipKey(const FKey& Key) const
{
	return Key == EKeys::Escape || (ToggleKey.IsValid() && Key == ToggleKey);
}

void URetrieveCreditsWidget::CompleteCredits(bool bSkipped)
{
	if (bCompleted)
	{
		return;
	}
	bCompleted = true;

	if (!bSkipped)
	{
		OnCreditsFinished();
	}
	OnCreditsCompleted.Broadcast(bSkipped);
}
