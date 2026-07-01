#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RetrieveSubtitleLineWidget.generated.h"

class UTextBlock;

/**
 * 자막 한 줄을 그리는 표시 전용 위젯 (스피커 이름 + 대사 + 페이드 인/아웃)
 * "무엇을, 언제" 보여줄지는 전적으로 호출자가 정하고,
 * 이 위젯은 받은 텍스트를 세팅하고 페이드 애니메이션만 재생합니다 (ViewModel 없음).
 * W_Bark와 시네마틱 자막(W_DialogueBox)이 공유합니다.
 */
UCLASS()
class RETRIEVE_API URetrieveSubtitleLineWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 텍스트/색 적용(페이드인 전에 호출) */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Bark")
	void SetLine(const FText& InSpeaker, const FText& InLine, FLinearColor InNameColor, FLinearColor InAccentColor);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Bark")
	void PlayFadeIn();

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Bark")
	void PlayFadeOut();

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleFadeOutFinished();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SpeakerNameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> LineText;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> FadeInAnim;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> FadeOutAnim;
};
