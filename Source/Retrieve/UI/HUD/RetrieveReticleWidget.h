#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Engine/TimerHandle.h"
#include "RetrieveReticleWidget.generated.h"

class UReticleViewModel;

/**
 * 레티클 위젯 베이스. 자식 WBP라 HUD 상단 주입이 닿지 않으므로,
 * QuestTracker와 동일하게 NativeConstruct에서 스스로 Reticle VM을 자기 뷰에 주입한다.
 * WBP_Reticle의 부모 클래스로 지정할 것.
 */
UCLASS()
class RETRIEVE_API URetrieveReticleWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// WBP가 구현: 스프레드 값(0~1)을 기존 SetAimValue(애니 스크럽)로 반영한다.
	// 0 = 무차징, 1 = 풀차징. 시각 매핑(조여듦/벌어짐)은 WBP 쪽에서 결정.
	UFUNCTION(BlueprintImplementableEvent, Category = "Retrieve|UI")
	void SetAimSpread(float Spread);

	// 차징 스프레드 갱신 주기(초). 0.01 = 100Hz. 매 프레임 틱 대신 이 간격으로만 갱신.
	UPROPERTY(EditAnywhere, Category = "Retrieve|UI", meta = (ClampMin = "0.001"))
	float ChargeUpdateInterval = 0.01f;

private:
	UFUNCTION()
	void HandleChargeStarted(float MaxChargeTime);

	UFUNCTION()
	void HandleChargeEnded(bool bReleased, float ChargeRatio);

	// 차징 램프 구간에만 도는 루프 타이머 콜백. 풀차징 도달 시 타이머 해제.
	void TickChargeTimer();

	UPROPERTY(Transient)
	TObjectPtr<UReticleViewModel> BoundViewModel;

	FTimerHandle ChargeTimerHandle;
	float ChargeStartTime = 0.f;
};