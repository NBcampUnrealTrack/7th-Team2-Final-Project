#include "UI/HUD/RetrieveBuffBarWidget.h"

#include "Components/PanelWidget.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "UI/HUD/RetrieveBuffSlotWidget.h"

// ─────────────────────────── 생명주기 ─────────────────────────────────────────

void URetrieveBuffBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	InitSlotPool();

	if (UWorld* World = GetWorld())
	{
		UGameplayMessageSubsystem& Msg = UGameplayMessageSubsystem::Get(World);
		ApplyHandle = Msg.RegisterListener<FRetrieveUIBuffPayload>(
			RetrieveGameplayTags::Channel_UI_Buff_Apply,
			[this](FGameplayTag Channel, const FRetrieveUIBuffPayload& Payload)
			{
				OnBuffApply(Channel, Payload);
			});
		RemoveHandle = Msg.RegisterListener<FRetrieveUIBuffRemovePayload>(
			RetrieveGameplayTags::Channel_UI_Buff_Remove,
			[this](FGameplayTag Channel, const FRetrieveUIBuffRemovePayload& Payload)
			{
				OnBuffRemove(Channel, Payload);
			});
	}

	TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::TickAllSlots));
}

void URetrieveBuffBarWidget::NativeDestruct()
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}

	if (UWorld* World = GetWorld())
	{
		UGameplayMessageSubsystem& Msg = UGameplayMessageSubsystem::Get(World);
		if (ApplyHandle.IsValid())
		{
			Msg.UnregisterListener(ApplyHandle);
			ApplyHandle = FGameplayMessageListenerHandle();
		}
		if (RemoveHandle.IsValid())
		{
			Msg.UnregisterListener(RemoveHandle);
			RemoveHandle = FGameplayMessageListenerHandle();
		}
	}

	Super::NativeDestruct();
}

// ─────────────────────────── 초기화 ───────────────────────────────────────────

void URetrieveBuffBarWidget::InitSlotPool()
{
	if (!BuffSlotClass || !HB_Slots) return;

	HB_Slots->ClearChildren();
	SlotPool.Reset();
	SlotRemaining.Reset();
	ActiveBuffToSlot.Reset();

	SlotPool.Reserve(MaxSlots);
	SlotRemaining.Init(0.f, MaxSlots);

	for (int32 i = 0; i < MaxSlots; ++i)
	{
		URetrieveBuffSlotWidget* NewSlot = CreateWidget<URetrieveBuffSlotWidget>(this, BuffSlotClass);
		if (NewSlot)
		{
			NewSlot->ClearBuff();
			HB_Slots->AddChild(NewSlot);
			SlotPool.Add(NewSlot);
		}
	}
}

// ─────────────────────────── 메시지 핸들러 ────────────────────────────────────

void URetrieveBuffBarWidget::OnBuffApply(FGameplayTag /*Channel*/, const FRetrieveUIBuffPayload& Payload)
{
	// 동일 BuffId가 이미 활성 → 내용만 갱신
	if (int32* ExistingIdx = ActiveBuffToSlot.Find(Payload.BuffId))
	{
		SlotPool[*ExistingIdx]->SetBuff(Payload);
		SlotRemaining[*ExistingIdx] = Payload.Duration;
		return;
	}

	const int32 FreeIdx = FindFreeSlot();
	if (FreeIdx == INDEX_NONE) return;

	ActiveBuffToSlot.Add(Payload.BuffId, FreeIdx);
	SlotPool[FreeIdx]->SetBuff(Payload);
	SlotRemaining[FreeIdx] = Payload.Duration;
}

void URetrieveBuffBarWidget::OnBuffRemove(FGameplayTag /*Channel*/, const FRetrieveUIBuffRemovePayload& Payload)
{
	if (const int32* Idx = ActiveBuffToSlot.Find(Payload.BuffId))
	{
		ClearSlot(*Idx);
	}
}

// ─────────────────────────── 틱 ───────────────────────────────────────────────

bool URetrieveBuffBarWidget::TickAllSlots(float DeltaTime)
{
	for (auto It = ActiveBuffToSlot.CreateIterator(); It; ++It)
	{
		const int32 Idx = It->Value;
		if (!SlotRemaining.IsValidIndex(Idx) || SlotRemaining[Idx] <= 0.f)
		{
			continue;
		}

		SlotRemaining[Idx] -= DeltaTime;

		if (SlotRemaining[Idx] <= 0.f)
		{
			SlotPool[Idx]->ClearBuff();
			SlotRemaining[Idx] = 0.f;
			It.RemoveCurrent();
		}
		else
		{
			SlotPool[Idx]->UpdateDuration(SlotRemaining[Idx]);
		}
	}
	return true;
}

// ─────────────────────────── 유틸 ─────────────────────────────────────────────

int32 URetrieveBuffBarWidget::FindFreeSlot() const
{
	for (int32 i = 0; i < SlotPool.Num(); ++i)
	{
		if (!SlotPool[i]->IsActive())
		{
			return i;
		}
	}
	return INDEX_NONE;
}

void URetrieveBuffBarWidget::ClearSlot(int32 SlotIndex)
{
	if (!SlotPool.IsValidIndex(SlotIndex)) return;

	const FGameplayTag BuffId = SlotPool[SlotIndex]->GetActiveBuffId();
	SlotPool[SlotIndex]->ClearBuff();
	SlotRemaining[SlotIndex] = 0.f;
	ActiveBuffToSlot.Remove(BuffId);
}
