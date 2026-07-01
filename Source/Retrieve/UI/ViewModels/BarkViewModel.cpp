#include "BarkViewModel.h"

#include "Bark/BarkStyleAsset.h"
#include "GameplayTags/RetrieveGameplayTags.h"

void UBarkViewModel::Initialize(UWorld* InWorld, UBarkStyleAsset* InStyle)
{
	WorldPtr = InWorld;
	Style = InStyle;
	if (!InWorld)
	{
		return;
	}

	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(InWorld);
	BarkHandle = MessageSubsystem.RegisterListener<FRetrieveBarkPayload>(
		RetrieveGameplayTags::Channel_UI_BarkRequested, this, &UBarkViewModel::HandleBarkRequested);
	CinematicHandle = MessageSubsystem.RegisterListener<FRetrieveCinematicStatePayload>(
		RetrieveGameplayTags::Channel_Cinematic_Changed, this, &UBarkViewModel::HandleCinematicChanged);
	DialogueHandle = MessageSubsystem.RegisterListener<FRetrieveDialogueChangedPayload>(
		RetrieveGameplayTags::Channel_UI_DialogueChanged, this, &UBarkViewModel::HandleDialogueChanged);
}

void UBarkViewModel::Shutdown()
{
	ClearActiveTicker();
	if (UWorld* World = WorldPtr.Get())
	{
		UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(World);
		MessageSubsystem.UnregisterListener(BarkHandle);
		MessageSubsystem.UnregisterListener(CinematicHandle);
		MessageSubsystem.UnregisterListener(DialogueHandle);
	}
	Queue.Reset();
}

void UBarkViewModel::HandleBarkRequested(FGameplayTag Channel, const FRetrieveBarkPayload& Payload)
{
	Enqueue(Payload);
}

void UBarkViewModel::HandleCinematicChanged(FGameplayTag Channel, const FRetrieveCinematicStatePayload& Payload)
{
	bCinematicActive = Payload.bActive;
	OnSuppressionChanged();
}

void UBarkViewModel::HandleDialogueChanged(FGameplayTag Channel, const FRetrieveDialogueChangedPayload& Payload)
{
	bDialogueActive = Payload.bActive;
	OnSuppressionChanged();
}

void UBarkViewModel::Enqueue(const FRetrieveBarkPayload& Payload)
{
	Queue.Add(Payload);
	while (Queue.Num() > FMath::Max(1, MaxQueued))
	{
		Queue.RemoveAt(0); // 가장 오래된 것부터 버림
	}
	if (!bPlaying && !IsSuppressed())
	{
		PlayNext();
	}
}

void UBarkViewModel::PlayNext()
{
	ClearActiveTicker();

	if (IsSuppressed() || Queue.Num() == 0)
	{
		bPlaying = false;
		return;
	}

	const FRetrieveBarkPayload Payload = Queue[0];
	Queue.RemoveAt(0);

	// 스타일 적용
	NameColor = FLinearColor::White;
	AccentColor = FLinearColor::White;
	if (Style)
	{
		FBarkStyleRow StyleRow;
		if (Style->GetStyle(Payload.SpeakerTag, StyleRow))
		{
			NameColor = StyleRow.NameColor;
			AccentColor = StyleRow.AccentColor;
		}
	}

	SpeakerName = Payload.SpeakerName;
	LineText = Payload.Line;
	bLineVisible = true;
	bPlaying = true;

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetSpeakerName);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetLineText);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetNameColor);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetAccentColor);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetIsLineVisible);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetSlateVisibility);

	OnShowLine.Broadcast(); // 위젯이 자막에 텍스트/색 적용 후 페이드인

	const float Hold = FMath::Max(0.5f, Payload.Duration);
	ActiveTicker = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UBarkViewModel::OnHoldExpired), Hold);
}

bool UBarkViewModel::OnHoldExpired(float)
{
	ActiveTicker.Reset();
	OnHideLine.Broadcast(); // 위젯 페이드아웃 시작
	ActiveTicker = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UBarkViewModel::OnFadeOutDone), FMath::Max(0.f, FadeOutSeconds));
	return false;
}

bool UBarkViewModel::OnFadeOutDone(float)
{
	ActiveTicker.Reset();
	bLineVisible = false;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetIsLineVisible);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetSlateVisibility);
	PlayNext();
	return false;
}

void UBarkViewModel::OnSuppressionChanged()
{
	if (IsSuppressed())
	{
		// 억제 시작: 진행 중 자막 즉시 숨김 + 정지 (앰비언트 생성은 서브시스템이 멈춤, 이벤트 행은 큐에 남아 유예 후 재생)
		ClearActiveTicker();
		if (bLineVisible)
		{
			OnHideLine.Broadcast();
			bLineVisible = false;
			UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetIsLineVisible);
			UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetSlateVisibility);
		}
		bPlaying = false;
	}
	else if (!bPlaying && Queue.Num() > 0)
	{
		// 억제 해제: GraceSeconds 이후 재개
		ClearActiveTicker();
		ActiveTicker = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &UBarkViewModel::OnGraceElapsed), FMath::Max(0.f, GraceSeconds));
	}
}

bool UBarkViewModel::OnGraceElapsed(float)
{
	ActiveTicker.Reset();
	PlayNext();
	return false;
}

void UBarkViewModel::ClearActiveTicker()
{
	if (ActiveTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ActiveTicker);
		ActiveTicker.Reset();
	}
}
