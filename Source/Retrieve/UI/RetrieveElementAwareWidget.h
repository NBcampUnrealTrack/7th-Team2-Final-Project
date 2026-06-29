#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "RetrieveElementAwareWidget.generated.h"

class UElementGaugeViewModel;

/**
 * ElementGaugeViewModel::OnCurrentElementChanged를 구독하고
 * BP_OnElementModeChanged Blueprint 이벤트로 dispatch하는 공통 베이스 위젯.
 *
 * RetrieveElementGaugeWidget(WBP_ElementGauge)과
 * RetrieveUIVFXWidget → RetrieveGamePanelWidget(WBP_ElementMode)이
 * 코드 중복 없이 원소 모드 변경 이벤트를 수신한다.
 */
UCLASS(Abstract, Blueprintable)
class RETRIEVE_API URetrieveElementAwareWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/**
	 * 원소 모드가 변경될 때 호출된다. NativeConstruct 시 초기값으로도 1회 호출된다.
	 * 기본 구현: BP_OnElementModeChanged Blueprint 이벤트로 dispatch.
	 * 서브클래스에서 오버라이드 후 Super 호출로 dispatch를 위임할 수 있다.
	 */
	virtual void NativeOnElementModeChanged(FGameplayTag NewElement);

	/** BP Event Construct 시점에 현재 활성 원소 태그를 조회한다. */
	UFUNCTION(BlueprintPure, Category = "Retrieve|ElementAware")
	FGameplayTag GetCurrentActiveElement() const;

private:
	TWeakObjectPtr<UElementGaugeViewModel> BoundElementVM;

	UFUNCTION()
	void HandleElementModeChanged(FGameplayTag NewElement);

	void DispatchElementModeChangedToBlueprint(FGameplayTag NewElement);
};
