#include "UI/RetrieveInteractionPromptWidget.h"

#include "UI/RetrieveUISettingsLibrary.h"

void URetrieveInteractionPromptWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// "HUD 숨기기"가 켜져 있으면 프롬프트를 시각적으로만 숨긴다(투명도 0). Visibility는 그대로 두어
	// 위젯이 계속 틱/기능하고, 플러그인의 표시 로직과도 충돌하지 않는다.
	const bool bHidden = URetrieveUISettingsLibrary::IsHUDHidden(this);

	// 상태가 바뀔 때만 조정한다. 매 틱 강제하면 SlideUp/Flash 등 프롬프트 자체 페이드 연출과 충돌한다.
	if (!bPromptHiddenInitialized || bHidden != bPromptHiddenApplied)
	{
		bPromptHiddenInitialized = true;
		bPromptHiddenApplied = bHidden;
		SetRenderOpacity(bHidden ? 0.f : 1.f);
	}
}
