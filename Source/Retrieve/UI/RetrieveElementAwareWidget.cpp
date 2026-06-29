#include "UI/RetrieveElementAwareWidget.h"
#include "Player/RetrievePlayerController.h"
#include "UI/ViewModels/HUDViewModel.h"
#include "UI/ViewModels/ElementGaugeViewModel.h"

void URetrieveElementAwareWidget::NativeConstruct()
{
	// BP Event Construct 이전에 구독 → GetCurrentActiveElement()가 Construct 시점에 유효
	ARetrievePlayerController* RPC = Cast<ARetrievePlayerController>(GetOwningPlayer());
	UE_LOG(LogTemp, Warning, TEXT("[ElementAware] NativeConstruct: Widget=%s  RPC=%s"),
		*GetName(), RPC ? *RPC->GetName() : TEXT("NULL"));
	if (RPC)
	{
		UHUDViewModel* HUDVM = RPC->GetHUDViewModel();
		UE_LOG(LogTemp, Warning, TEXT("[ElementAware] HUDVM=%s"), HUDVM ? *HUDVM->GetName() : TEXT("NULL"));
		if (HUDVM)
		{
			UElementGaugeViewModel* GaugeVM = HUDVM->GetElementGauge();
			UE_LOG(LogTemp, Warning, TEXT("[ElementAware] GaugeVM=%s"), GaugeVM ? *GaugeVM->GetName() : TEXT("NULL"));
			if (GaugeVM)
			{
				BoundElementVM = GaugeVM;
				GaugeVM->OnCurrentElementChanged.AddDynamic(this, &ThisClass::HandleElementModeChanged);
				UE_LOG(LogTemp, Warning, TEXT("[ElementAware] Subscribed OK. CurrentElement=%s"),
					*GaugeVM->GetCurrentElement().ToString());
			}
		}
	}

	Super::NativeConstruct(); // BP Event Construct 발화 — 이때 GetCurrentActiveElement() 사용 가능

	// BP Construct 완료 후 초기 원소 dispatch
	if (UElementGaugeViewModel* VM = BoundElementVM.Get())
	{
		NativeOnElementModeChanged(VM->GetCurrentElement());
	}
}

FGameplayTag URetrieveElementAwareWidget::GetCurrentActiveElement() const
{
	if (const UElementGaugeViewModel* VM = BoundElementVM.Get())
	{
		return VM->GetCurrentElement();
	}
	return FGameplayTag::EmptyTag;
}

void URetrieveElementAwareWidget::NativeDestruct()
{
	if (UElementGaugeViewModel* VM = BoundElementVM.Get())
	{
		VM->OnCurrentElementChanged.RemoveDynamic(this, &ThisClass::HandleElementModeChanged);
	}
	Super::NativeDestruct();
}

void URetrieveElementAwareWidget::HandleElementModeChanged(FGameplayTag NewElement)
{
	NativeOnElementModeChanged(NewElement);
}

void URetrieveElementAwareWidget::NativeOnElementModeChanged(FGameplayTag NewElement)
{
	DispatchElementModeChangedToBlueprint(NewElement);
}

void URetrieveElementAwareWidget::DispatchElementModeChangedToBlueprint(FGameplayTag NewElement)
{
	UFunction* EventFunction = FindFunction(TEXT("BP_OnElementModeChanged"));
	UE_LOG(LogTemp, Warning, TEXT("[ElementAware] Dispatch: Widget=%s  Element=%s  Func=%s"),
		*GetName(), *NewElement.ToString(), EventFunction ? TEXT("FOUND") : TEXT("NOT FOUND"));
	if (!EventFunction)
	{
		return;
	}

	struct FParams { FGameplayTag NewElementTag; };
	FParams Params;
	Params.NewElementTag = NewElement;
	ProcessEvent(EventFunction, &Params);
}
