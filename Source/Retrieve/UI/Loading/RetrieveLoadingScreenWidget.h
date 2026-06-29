#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RetrieveLoadingScreenWidget.generated.h"

/** 전체 화면 로딩/전환 커버. 선택적 페이드아웃 애니메이션 재생 후 스스로 제거됩니다. */
UCLASS()
class RETRIEVE_API URetrieveLoadingScreenWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	/** FadeOutAnim이 있으면 재생하고 종료 시 부모에서 제거; 없으면 즉시 제거합니다. */
	void PlayFadeOutAndRemove();

protected:
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> FadeOutAnim;

	UFUNCTION()
	void HandleFadeOutFinished();
};
