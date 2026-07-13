#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RetrieveInteractionPromptWidget.generated.h"

/**
 * 상호작용 프롬프트(WB_InteractionTarget)의 C++ 베이스.
 *
 * "HUD 숨기기" 설정이 켜지면 프롬프트를 시각적으로만 숨긴다(RenderOpacity=0).
 * 가시성(Visibility)은 건드리지 않으므로 InteractionManager 플러그인의 표시/숨김 로직과
 * 충돌하지 않고, 상호작용 감지·입력 등 기능은 그대로 동작한다("숨김 표시만").
 *
 * WB_InteractionTarget(부모=UserWidget)을 이 클래스로 reparent 해서 사용한다.
 */
UCLASS()
class RETRIEVE_API URetrieveInteractionPromptWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	/** 마지막으로 적용한 숨김 상태. 상태 전환 시에만 RenderOpacity를 조정한다(페이드 애니메이션과의 충돌 방지). */
	bool bPromptHiddenApplied = false;
	bool bPromptHiddenInitialized = false;
};
