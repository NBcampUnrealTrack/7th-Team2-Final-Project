#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RetrieveLoadingScreenWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRetrieveLoadingScreenRemoved);

/** 전체 화면 로딩/전환 커버. 선택적 페이드아웃 애니메이션 재생 후 스스로 제거됩니다. */
UCLASS()
class RETRIEVE_API URetrieveLoadingScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * FadeOutAnim(있으면)을 재생하고 종료 시 부모에서 제거; 없으면 즉시 제거합니다.
	 * 여러 경로에서 호출될 수 있어 최초 1회만 동작합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI")
	void PlayFadeOutAndRemove();

	UPROPERTY(BlueprintAssignable)
	FRetrieveLoadingScreenRemoved OnRemoved;

	/* 커버가 완전히 불투명해지기까지 걸리는 시간(초). WBP 애니메이션의 페이드인 구간 길이와 같은 값으로 맞추세요. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|UI")
	float CoverFadeInSeconds = 0.4f;

	UFUNCTION(BlueprintPure, Category = "Retrieve|UI")
	float GetCoverFadeInSeconds() const { return CoverFadeInSeconds; }

	// ── 목표 브리핑 ───────────────────────────────────────────────────────────
	// 로딩 화면은 플레이어가 어차피 보고 있는 유일한 정적 화면이다.
	// 여기에 "지금 목표"와 조작 팁을 얹어 "뭘 해야 할지 모르겠다"를 가장 싸게 줄인다.

	/** WBP에 같은 이름의 TextBlock을 두면 현재 목표가 자동으로 채워진다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> Text_Objective;

	/** 로딩마다 무작위로 한 줄 보여줄 팁. 비우면 팁 영역은 숨겨진다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> Text_Tip;

	/** 표시할 팁 문구들. 디자이너가 자유롭게 추가한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|UI|Briefing", meta = (MultiLine = true))
	TArray<FText> Tips;

protected:
	virtual void NativeConstruct() override;
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> FadeOutAnim;

	UFUNCTION()
	void HandleFadeOutFinished();

	/** PlayFadeOutAndRemove 중복 호출 가드 (페이드아웃/제거가 한 번만 일어나도록). */
	bool bFadeOutStarted = false;
};
