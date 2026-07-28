#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "EnemySuspicionIndicatorComponent.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class EEnemyAlertUIState : uint8
{
	None,        // 평상시, 아이콘 숨김
	Suspicious,  // 반투명 물음표 위로 선명한 물음표가 Gauge만큼 차오름
	Alerted,     // 느낌표 팝업
};

// 경계(Suspicious) 게이지를 ?/! 아이콘으로 표시하는 위젯 컴포넌트.
// RetrieveEnemyTargetEvaluator가 매 틱 SetSuspicionGauge()로 값을 밀어넣는다.
UCLASS(ClassGroup = (Retrieve), meta = (BlueprintSpawnableComponent))
class RETRIEVE_API UEnemySuspicionIndicatorComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UEnemySuspicionIndicatorComponent();

	// Evaluator가 매 틱 호출 — 값을 저장하고, 위젯에 SetIcons(최초 1회)/UpdateGauge를 직접 호출해 전달한다.
	void SetSuspicionGauge(float InGauge);

	UFUNCTION(BlueprintPure, Category = "Retrieve|AI")
	float GetSuspicionGauge() const { return SuspicionGauge; }

	UFUNCTION(BlueprintPure, Category = "Retrieve|AI")
	EEnemyAlertUIState GetAlertUIState() const
	{
		if (SuspicionGauge <= 0.f) return EEnemyAlertUIState::None;
		return SuspicionGauge >= 1.f ? EEnemyAlertUIState::Alerted : EEnemyAlertUIState::Suspicious;
	}

	// 몬스터/지역별로 다른 아이콘을 쓸 수 있도록 BP 인스턴스에서 직접 지정.
	// Suspicious 상태에선 Base(반투명) 위에 Fill(선명)을 Gauge로 마스킹해 겹쳐 그린다 — WBP 담당.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|AI|Icon")
	TObjectPtr<UTexture2D> SuspiciousBaseIcon;   // 반투명 물음표 (고정 배경)

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|AI|Icon")
	TObjectPtr<UTexture2D> SuspiciousFillIcon;   // 선명 물음표 (Gauge만큼 Fill 마스크로 차오름)

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|AI|Icon")
	TObjectPtr<UTexture2D> AlertedIcon;          // 느낌표 (팝업)

	// Suspicious→Alerted 전환 시 !를 보여주고 유지하는 시간(초). 0이면 즉시 숨김.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|AI", meta = (ClampMin = "0.0"))
	float AlertedDisplayDuration = 0.5f;

private:
	void HandleAlertedHideTimer();

	float SuspicionGauge = 0.f;
	bool bIconsPushed = false;

	FTimerHandle AlertedHideTimerHandle;
};
