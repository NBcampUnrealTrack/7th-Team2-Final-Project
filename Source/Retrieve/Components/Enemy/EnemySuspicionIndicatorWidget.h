#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Enemy/EnemySuspicionIndicatorComponent.h"
#include "EnemySuspicionIndicatorWidget.generated.h"

class UTexture2D;

// UEnemySuspicionIndicatorComponent가 값을 직접 밀어넣는 위젯 베이스.
// WidgetComponent가 만드는 월드 위젯은 소유 플레이어가 없어 Get Owning Actor류 노드를 못 쓴다 —
// 그래서 위젯이 컴포넌트를 찾아가지 않고, 컴포넌트가 아래 이벤트를 호출해 값을 전달한다.
UCLASS()
class RETRIEVE_API UEnemySuspicionIndicatorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 몬스터별 텍스처 3장 전달 (최초 1회)
	UFUNCTION(BlueprintImplementableEvent, Category = "Retrieve|UI")
	void SetIcons(UTexture2D* BaseIcon, UTexture2D* FillIcon, UTexture2D* AlertIcon);

	// Evaluator Tick마다(약 0.2초 간격) 현재 Gauge/상태 전달
	UFUNCTION(BlueprintImplementableEvent, Category = "Retrieve|UI")
	void UpdateGauge(float Gauge, EEnemyAlertUIState State);
};
