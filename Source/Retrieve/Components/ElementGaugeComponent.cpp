#include "Components/ElementGaugeComponent.h"

#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Components/WeaponComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Engine/DataTable.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "Player/RetrievePlayerState.h"

UElementGaugeComponent::UElementGaugeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	ElementSlots.Init(FElementSlot(), SlotCount);
	CurrentSlotIndex = 0;
}

void UElementGaugeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ItemMultiplierTimer);
	}
	UnbindFromASC();
	Super::EndPlay(EndPlayReason);
}

void UElementGaugeComponent::BindToASC()
{
	URetrieveAbilitySystemComponent* RetrieveASC = GetRetrieveASC();
	if (!RetrieveASC || !ChargeRuleTable)
	{
		return;
	}

	if (GameplayEventHandle.IsValid())
	{
		return;
	}

	// 캐시 빌드 (테이블 → TMap)
	ChargeRuleCache.Reset();
	SubscribedFilter.Reset();

	static const FString Context(TEXT("UElementGaugeComponent::BindToASC"));
	TArray<FElementChargeRule*> Rows;
	ChargeRuleTable->GetAllRows<FElementChargeRule>(Context, Rows);
	for (const FElementChargeRule* Row : Rows)
	{
		if (!Row || !Row->EventTag.IsValid() || Row->ChargeAmount <= 0)
		{
			continue;
		}
		ChargeRuleCache.Add(Row->EventTag, Row->ChargeAmount);
		SubscribedFilter.AddTag(Row->EventTag);
	}

	if (SubscribedFilter.IsEmpty())
	{
		return;
	}

	GameplayEventHandle = RetrieveASC->AddGameplayEventTagContainerDelegate(
		SubscribedFilter,
		FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(this, &UElementGaugeComponent::HandleGameplayEvent));
}

void UElementGaugeComponent::UnbindFromASC()
{
	if (!GameplayEventHandle.IsValid())
	{
		return;
	}

	if (ASC.IsValid())
	{
		ASC->RemoveGameplayEventTagContainerDelegate(SubscribedFilter, GameplayEventHandle);
	}

	GameplayEventHandle.Reset();
	SubscribedFilter.Reset();
	ChargeRuleCache.Reset();
}

void UElementGaugeComponent::HandleGameplayEvent(FGameplayTag EventTag, const FGameplayEventData* Payload)
{
	// 본인이 발생시킨 이벤트만 게이지 충전
	if (!Payload || Payload->Instigator != GetOwner())
	{
		return;
	}

	if (const int32* Amount = ChargeRuleCache.Find(EventTag))
	{
		AddCharge(*Amount);
	}
}

void UElementGaugeComponent::AddCharge(int32 Amount)
{
	// 배율은 진입부에서 1회만 적용하고, 내부 재귀(AddChargeInternal)에는 전달하지 않는다.
	// 무기 배율 × 아이템 버프 배율을 함께 적용한다.
	float Multiplier = ItemChargeMultiplier;
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (const UWeaponComponent* Weapon = OwnerPawn->FindComponentByClass<UWeaponComponent>())
		{
			Multiplier *= FMath::Max(Weapon->GetWeaponData().ElementChargeMultiplier, 0.f);
		}
	}

	AddChargeInternal(FMath::RoundToInt(Amount * Multiplier));
}

void UElementGaugeComponent::SetItemChargeMultiplier(float Multiplier, float Duration, FGameplayTag BuffUITag)
{
	ItemChargeMultiplier = FMath::Max(Multiplier, 0.f);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ItemMultiplierTimer);

		if (Duration > 0.f)
		{
			World->GetTimerManager().SetTimer(
				ItemMultiplierTimer,
				[this, BuffUITag]()
				{
					ItemChargeMultiplier = 1.f;
					if (BuffUITag.IsValid())
					{
						if (UWorld* TimerWorld = GetWorld())
						{
							FRetrieveUIBuffRemovePayload RemovePayload;
							RemovePayload.BuffId = BuffUITag;
							UGameplayMessageSubsystem::Get(TimerWorld).BroadcastMessage(
								RetrieveGameplayTags::Channel_UI_Buff_Remove, RemovePayload);
						}
					}
				},
				Duration,
				false);
		}
	}
}

void UElementGaugeComponent::AddChargeInternal(int32 ScaledAmount)
{
	if (IsFull() || ScaledAmount <= 0) return;

	int32 SumAmount = ElementSlots[CurrentSlotIndex].InternalGauge + ScaledAmount;
	if (SumAmount >= ElementSlots[CurrentSlotIndex].MaxGauge)
	{
		int32 LeftAmount = SumAmount - ElementSlots[CurrentSlotIndex].MaxGauge;
		CommitSlot();
		AddChargeInternal(LeftAmount);
	}
	else
	{
		ElementSlots[CurrentSlotIndex].InternalGauge = SumAmount;

		// 충전 중에도 현재 원소를 반영해 색상이 즉시 표시되도록 한다.
		if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
		{
			if (ARetrievePlayerState* PS = OwnerPawn->GetPlayerState<ARetrievePlayerState>())
			{
				ElementSlots[CurrentSlotIndex].CurrentElement = PS->GetCurrentElementTag();
			}
		}

		OnSlotsChanged.Broadcast();
	}
}

void UElementGaugeComponent::CommitSlot()
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn) return;

	ARetrievePlayerState* PS = OwnerPawn->GetPlayerState<ARetrievePlayerState>();
	if (!PS) return;

	ElementSlots[CurrentSlotIndex].InternalGauge = ElementSlots[CurrentSlotIndex].MaxGauge;
	ElementSlots[CurrentSlotIndex].bFull = true;
	ElementSlots[CurrentSlotIndex].CurrentElement = PS->GetCurrentElementTag();

	CurrentSlotIndex++;
	OnSlotsChanged.Broadcast();

	if (IsFull())
	{
		if (URetrieveAbilitySystemComponent* RetrieveASC = GetRetrieveASC())
		{
			RetrieveASC->AddLooseGameplayTag(RetrieveGameplayTags::State_Gauge_Full);
		}
		BroadcastGaugeFull();
	}
}

void UElementGaugeComponent::BroadcastGaugeFull() const
{
	if (!GetWorld())
	{
		return;
	}

	FRetrieveElementGaugeFullPayload Payload;
	Payload.Instigator = GetOwner();
	for (const FElementSlot& Slot : ElementSlots)
	{
		if (Slot.bFull)
		{
			Payload.FilledElements.Add(Slot.CurrentElement);
		}
	}

	UGameplayMessageSubsystem::Get(GetWorld())
		.BroadcastMessage(RetrieveGameplayTags::Channel_ElementGauge_Full, Payload);
}

bool UElementGaugeComponent::IsFull() const
{
	return CurrentSlotIndex >= SlotCount;
}

TMap<FGameplayTag, int32> UElementGaugeComponent::GetCurrentCombination() const
{
	TMap<FGameplayTag, int32> ElementPattern;

	for (const FElementSlot& Slot : ElementSlots)
	{
		if (Slot.bFull)
		{
			int32& Count = ElementPattern.FindOrAdd(Slot.CurrentElement, 0);
			Count++;
		}
	}

	return ElementPattern;
}

FGameplayTag UElementGaugeComponent::ConsumeOldestSlot()
{
	if (!ElementSlots[0].bFull) return RetrieveGameplayTags::Element_None;

	if (IsFull())
	{
		URetrieveAbilitySystemComponent* RetrieveASC = GetRetrieveASC();
		if (!RetrieveASC) return RetrieveGameplayTags::Element_None;

		RetrieveASC->SetLooseGameplayTagCount(RetrieveGameplayTags::State_Gauge_Full, 0);
	}

	FGameplayTag ElementTag = ElementSlots[0].CurrentElement;

	for (int32 i = 0; i < SlotCount - 1; ++i)
	{
		ElementSlots[i] = ElementSlots[i + 1];
	}

	ElementSlots[SlotCount - 1] = FElementSlot();

	CurrentSlotIndex--;
	OnSlotsChanged.Broadcast();

	return ElementTag;
}

FGameplayTag UElementGaugeComponent::PeekOldestSlot() const
{
	return ElementSlots[0].bFull ? ElementSlots[0].CurrentElement : RetrieveGameplayTags::Element_None;
}

void UElementGaugeComponent::ClearSlot()
{
	const bool bWasFull = IsFull();

	for (int32 i = 0; i < SlotCount; ++i)
	{
		ElementSlots[i] = FElementSlot();
	}

	CurrentSlotIndex = 0;

	if (bWasFull)
	{
		if (URetrieveAbilitySystemComponent* RetrieveASC = GetRetrieveASC())
		{
			RetrieveASC->SetLooseGameplayTagCount(RetrieveGameplayTags::State_Gauge_Full, 0);
		}
	}

	OnSlotsChanged.Broadcast();
}

float UElementGaugeComponent::GetSlotRatio(int32 SlotIndex) const
{
	if (!ElementSlots.IsValidIndex(SlotIndex)) return 0.f;

	const FElementSlot& Slot = ElementSlots[SlotIndex];
	return Slot.MaxGauge > 0 ? static_cast<float>(Slot.InternalGauge) / static_cast<float>(Slot.MaxGauge) : 0.f;
}

URetrieveAbilitySystemComponent* UElementGaugeComponent::GetRetrieveASC() const
{
	if (ASC.IsValid())
	{
		return ASC.Get();
	}

	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn) return nullptr;

	ARetrievePlayerState* PS = OwnerPawn->GetPlayerState<ARetrievePlayerState>();
	if (!PS) return nullptr;

	URetrieveAbilitySystemComponent* RetrieveASC = PS->GetRetrieveAbilitySystemComponent();
	ASC = RetrieveASC;
	return RetrieveASC;
}
