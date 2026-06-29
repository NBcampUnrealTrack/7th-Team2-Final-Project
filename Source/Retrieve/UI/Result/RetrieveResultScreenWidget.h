#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RetrieveResultScreenWidget.generated.h"

class UTextBlock;
class UButton;

/**
 * Result 상태의 최상위 위젯 (ARetrievePlayerController::ResultClass 필드)
 * 사망 전용: "YOU DIED"를 표시하고 화면을 검게 페이드한 후, 짧은 대기 뒤 Server_RequestRetry를 자동으로 실행
 */
UCLASS()
class RETRIEVE_API URetrieveResultScreenWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	/** 사망 대기 / 페이드 재생이 끝난 후 호스트 게이팅된 리스폰 요청을 실행함. */
	void RequestRespawn();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> FadeInAnim;

	UPROPERTY(EditAnywhere, Category = "Retrieve|Result")
	FText DeathTitleText = FText::FromString(TEXT("YOU DIED"));

	UPROPERTY(EditAnywhere, Category = "Retrieve|Result")
	float RespawnDelay = 3.0f;

	FTimerHandle RespawnTimerHandle;
};
