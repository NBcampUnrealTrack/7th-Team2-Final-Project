#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RetrieveTimedActionWidget.generated.h"

class UTextBlock;
class UProgressBar;

/**
 * 제작/강화 대기 연출 위젯. WBP_HUD_FantasyWarrior_TimedAction_01의 C++ 부모 클래스.
 *
 * WBP 구성 (기존 애셋 그대로 사용):
 *   TXT_Action     — 진행 중인 작업 이름 (Is Variable 체크됨, BindWidgetOptional로 자동 바인딩)
 *   ProgressBar_32 — 진행률 바 (Is Variable 미체크 → GetWidgetFromName으로 런타임 조회)
 */
UCLASS()
class RETRIEVE_API URetrieveTimedActionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 지정한 시간(초) 동안 진행 바를 채우고, 끝나면 OnComplete를 호출한다. */
	void StartTimedAction(float Duration, const FText& ActionText, FSimpleDelegate OnComplete);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TXT_Action;

private:
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> ProgressBar_32;

	FSimpleDelegate CompletionCallback;
	float TotalDuration = 0.0f;
	float ElapsedTime = 0.0f;
	bool bIsRunning = false;
};
