#include "Components/ElementGaugeComponent.h"

#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Player/RetrievePlayerState.h"

UElementGaugeComponent::UElementGaugeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	ElementSlots.Init(FElementSlot(), SlotCount);
	CurrentSlotIndex = 0;
}

void UElementGaugeComponent::AddCharge(int32 Amount)
{
	if (IsFull()) return;

	int32 SumAmount = ElementSlots[CurrentSlotIndex].InternalGauge + Amount;
	if (SumAmount >= ElementSlots[CurrentSlotIndex].MaxGauge)
	{
		int32 LeftAmount = SumAmount - ElementSlots[CurrentSlotIndex].MaxGauge;
		CommitSlot();
		AddCharge(LeftAmount);
	}
	else
	{
		ElementSlots[CurrentSlotIndex].InternalGauge = SumAmount;

		UE_LOG(LogTemp, Warning, TEXT("Slot %d : %d%%"), CurrentSlotIndex+1, SumAmount);
	}
}

void UElementGaugeComponent::CommitSlot()
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn) return;

	ARetrievePlayerState* PS = OwnerPawn->GetPlayerState<ARetrievePlayerState>();
	if (!PS) return;

	ElementSlots[CurrentSlotIndex].InternalGauge = ElementSlots[CurrentSlotIndex].MaxGauge;
	ElementSlots[CurrentSlotIndex].bIsFull = true;
	ElementSlots[CurrentSlotIndex].CurrentElement = PS->GetCurrentElementTag();

	UE_LOG(LogTemp, Warning, TEXT("Slot %d Charged"), CurrentSlotIndex+1);
	CurrentSlotIndex++; 
	
	if (IsFull())
	{
		ASC = GetRetrieveASC();
		if (!ASC.IsValid()) return;

		ASC->AddLooseGameplayTag(RetrieveGameplayTags::State_Gauge_Full);
	}
}

bool UElementGaugeComponent::IsFull()
{
	return CurrentSlotIndex >= SlotCount;
}

TMap<FGameplayTag, int32> UElementGaugeComponent::GetCurrentCombination()
{
	TMap<FGameplayTag, int32> ElementPattern;

	for (const FElementSlot& Slot : ElementSlots)
	{
		if (Slot.bIsFull)
		{
			int32& Count = ElementPattern.FindOrAdd(Slot.CurrentElement, 0);
			Count++;
		}
	}

	return ElementPattern;
}

FGameplayTag UElementGaugeComponent::ConsumeOldestSlot()
{
	if (!ElementSlots[0].bIsFull) return RetrieveGameplayTags::Element_None;

	if (IsFull())
	{
		ASC = GetRetrieveASC();
		if (!ASC.IsValid()) return RetrieveGameplayTags::Element_None;

		ASC->SetLooseGameplayTagCount(RetrieveGameplayTags::State_Gauge_Full, 0);
	}

	FGameplayTag ElementTag = ElementSlots[0].CurrentElement;

	for (int32 i = 0; i < SlotCount - 1; ++i)
	{
		ElementSlots[i] = ElementSlots[i + 1];
	}

	ElementSlots[SlotCount - 1] = FElementSlot();

	UE_LOG(LogTemp, Warning, TEXT("Oldest Slot Consumed"));

	CurrentSlotIndex--;

	return ElementTag;
}

void UElementGaugeComponent::ConsumeAllSlots()
{
	ClearSlot();
}

void UElementGaugeComponent::ClearSlot()
{
	for (int32 i = 0; i < SlotCount; ++i)
	{
		ElementSlots[i] = FElementSlot();
	}

	if (IsFull())
	{
		ASC = GetRetrieveASC();
		if (!ASC.IsValid()) return;

		ASC->SetLooseGameplayTagCount(RetrieveGameplayTags::State_Gauge_Full, 0);
	}

	CurrentSlotIndex = 0;
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

