#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "GameplayTagContainer.h"
#include "ElementGaugeViewModel.generated.h"

class UElementGaugeComponent;
class UAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnElementGaugeSlotsUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnElementGaugeCurrentElementChanged, FGameplayTag, NewElement);

/**
 * 원소 게이지 UI 구동.
 * - UElementGaugeComponent::OnSlotsChanged → 슬롯 3개 Ratio/IsFull/Element 직접 노출
 * - ASC RegisterGameplayTagEvent(Element.*) → GetCurrentElement() FieldNotify
 * - OnSlotsUpdated → URetrieveElementGaugeWidget이 구독, Material Instance 파라미터 갱신
 */
UCLASS(BlueprintType)
class RETRIEVE_API UElementGaugeViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UElementGaugeViewModel();

	// 슬롯 데이터 갱신 완료 알림 — URetrieveElementGaugeWidget이 구독해 Material 파라미터 갱신
	UPROPERTY(BlueprintAssignable, Category = "Retrieve|UI")
	FOnElementGaugeSlotsUpdated OnSlotsUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|UI")
	FOnElementGaugeCurrentElementChanged OnCurrentElementChanged;

	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI")
	void BindToGauge(UElementGaugeComponent* InGauge);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI")
	void UnbindFromGauge();

	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI")
	void BindToASC(UAbilitySystemComponent* InASC);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI")
	void UnbindFromASC();

	// --- 현재 원소 모드 ---
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|UI")
	FGameplayTag GetCurrentElement() const { return CurrentElement; }

	// --- Slot 0 (ProgressBar 직접 바인딩용) ---
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|UI")
	float GetSlot0Ratio() const { return SlotRatios[0]; }

	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|UI")
	bool GetSlot0IsFull() const { return SlotFullFlags[0]; }

	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|UI")
	FGameplayTag GetSlot0Element() const { return CurrentElement; }

	// --- Slot 1 ---
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|UI")
	float GetSlot1Ratio() const { return SlotRatios[1]; }

	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|UI")
	bool GetSlot1IsFull() const { return SlotFullFlags[1]; }

	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|UI")
	FGameplayTag GetSlot1Element() const { return CurrentElement; }

	// --- Slot 2 ---
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|UI")
	float GetSlot2Ratio() const { return SlotRatios[2]; }

	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|UI")
	bool GetSlot2IsFull() const { return SlotFullFlags[2]; }

	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|UI")
	FGameplayTag GetSlot2Element() const { return CurrentElement; }

	// --- 슬롯 색상 (GameplayTag → FLinearColor 변환 결과, FillColorAndOpacity 직접 바인딩용) ---
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|UI")
	FLinearColor GetSlot0Color() const;

	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|UI")
	FLinearColor GetSlot1Color() const;

	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|UI")
	FLinearColor GetSlot2Color() const;

	// --- 전체 상태 ---
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|UI")
	bool GetIsGaugeFull() const { return bIsGaugeFull; }

	static constexpr int32 MaxSlots = 3;

private:
	UFUNCTION()
	void HandleSlotsChanged();

	void HandleElementTagChanged(FGameplayTag Tag, int32 NewCount);
	void RefreshCurrentElement();

	UPROPERTY()
	TWeakObjectPtr<UElementGaugeComponent> BoundGauge;

	UPROPERTY()
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;

	FGameplayTag CurrentElement;

	float        SlotRatios[MaxSlots]    = { 0.f, 0.f, 0.f };
	bool         SlotFullFlags[MaxSlots] = { false, false, false };
	bool         bIsGaugeFull = false;
};
