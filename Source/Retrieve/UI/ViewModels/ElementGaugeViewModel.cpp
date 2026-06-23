#include "UI/ViewModels/ElementGaugeViewModel.h"

#include "AbilitySystemComponent.h"
#include "Components/Element/ElementGaugeComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "UI/RetrieveElementUILibrary.h"

// ─────────────────────────── 생성자 ──────────────────────────────────────────

UElementGaugeViewModel::UElementGaugeViewModel() {}

// ─────────────────────────── 원소 → 색상 ─────────────────────────────────────

// 슬롯별 원소 구분을 없애고, 게이지 색은 현재 원소모드 하나로 통일한다.
FLinearColor UElementGaugeViewModel::GetSlot0Color() const { return URetrieveElementUILibrary::ElementTagToColor(CurrentElement); }
FLinearColor UElementGaugeViewModel::GetSlot1Color() const { return URetrieveElementUILibrary::ElementTagToColor(CurrentElement); }
FLinearColor UElementGaugeViewModel::GetSlot2Color() const { return URetrieveElementUILibrary::ElementTagToColor(CurrentElement); }

// ─────────────────────────────── 게이지 바인딩 ────────────────────────────────

void UElementGaugeViewModel::BindToGauge(UElementGaugeComponent* InGauge)
{
	UnbindFromGauge();
	if (!InGauge) return;

	BoundGauge = InGauge;
	InGauge->OnSlotsChanged.AddDynamic(this, &ThisClass::HandleSlotsChanged);
	HandleSlotsChanged();
}

void UElementGaugeViewModel::UnbindFromGauge()
{
	if (UElementGaugeComponent* Gauge = BoundGauge.Get())
	{
		Gauge->OnSlotsChanged.RemoveDynamic(this, &ThisClass::HandleSlotsChanged);
	}
	BoundGauge = nullptr;
}

// ──────────────────────────── ASC 원소 모드 바인딩 ────────────────────────────

void UElementGaugeViewModel::BindToASC(UAbilitySystemComponent* InASC)
{
	UnbindFromASC();
	if (!InASC) return;

	BoundASC = InASC;

	InASC->RegisterGameplayTagEvent(RetrieveGameplayTags::Element_Fire,  EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::HandleElementTagChanged);
	InASC->RegisterGameplayTagEvent(RetrieveGameplayTags::Element_Water, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::HandleElementTagChanged);
	InASC->RegisterGameplayTagEvent(RetrieveGameplayTags::Element_Wind,  EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::HandleElementTagChanged);

	RefreshCurrentElement();
}

void UElementGaugeViewModel::UnbindFromASC()
{
	if (UAbilitySystemComponent* ASC = BoundASC.Get())
	{
		ASC->RegisterGameplayTagEvent(RetrieveGameplayTags::Element_Fire,  EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
		ASC->RegisterGameplayTagEvent(RetrieveGameplayTags::Element_Water, EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
		ASC->RegisterGameplayTagEvent(RetrieveGameplayTags::Element_Wind,  EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
	}
	BoundASC = nullptr;
}

void UElementGaugeViewModel::HandleElementTagChanged(FGameplayTag /*Tag*/, int32 /*NewCount*/)
{
	RefreshCurrentElement();
}

void UElementGaugeViewModel::RefreshCurrentElement()
{
	UAbilitySystemComponent* ASC = BoundASC.Get();
	if (!ASC) return;

	FGameplayTag NewElement = RetrieveGameplayTags::Element_None;
	if      (ASC->HasMatchingGameplayTag(RetrieveGameplayTags::Element_Fire))  NewElement = RetrieveGameplayTags::Element_Fire;
	else if (ASC->HasMatchingGameplayTag(RetrieveGameplayTags::Element_Water)) NewElement = RetrieveGameplayTags::Element_Water;
	else if (ASC->HasMatchingGameplayTag(RetrieveGameplayTags::Element_Wind))  NewElement = RetrieveGameplayTags::Element_Wind;

	if (NewElement != CurrentElement)
	{
		CurrentElement = NewElement;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetCurrentElement);

		// 게이지 색이 현재 원소모드를 따르므로, 모드가 바뀌면 슬롯 색/원소도 갱신을 알린다.
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetSlot0Element);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetSlot1Element);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetSlot2Element);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetSlot0Color);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetSlot1Color);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetSlot2Color);

		OnCurrentElementChanged.Broadcast(CurrentElement);
	}
}

// ──────────────────────────── 슬롯 변경 핸들러 ───────────────────────────────

void UElementGaugeViewModel::HandleSlotsChanged()
{
	UElementGaugeComponent* Gauge = BoundGauge.Get();
	if (!Gauge) return;

	const TArray<FElementSlot>& Slots = Gauge->GetElementSlots();
	const int32 Count = FMath::Min(Slots.Num(), MaxSlots);

	for (int32 i = 0; i < Count; ++i)
	{
		SlotRatios[i]    = Gauge->GetSlotRatio(i);
		SlotFullFlags[i] = Slots[i].bFull;
	}

	const bool bNewFull     = Gauge->IsFull();
	const bool bFullChanged = (bNewFull != bIsGaugeFull);
	bIsGaugeFull = bNewFull;

	// WBP_ElementGauge ProgressBar 바인딩용 직접 브로드캐스트.
	// 슬롯 색/원소는 현재 원소모드만 따르므로 여기(충전/확정)서는 알리지 않고 RefreshCurrentElement에서만 갱신한다.
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetSlot0Ratio);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetSlot0IsFull);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetSlot1Ratio);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetSlot1IsFull);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetSlot2Ratio);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetSlot2IsFull);

	if (bFullChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetIsGaugeFull);
	}

	// ViewModel 처리 완료 후 위젯에 알림 → Material 파라미터(Percent, FillColor) 갱신
	OnSlotsUpdated.Broadcast();
}
