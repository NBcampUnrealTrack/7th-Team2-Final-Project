#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RetrieveBarkWidget.generated.h"

class URetrieveSubtitleLineWidget;
class UBarkViewModel;
class UBarkStyleAsset;

/**
 * 화면 하단 중앙에 Bark 자막을 띄우는 호스트 위젯.
 * - W_HUD의 자식으로 배치되어, HUD가 숨겨지면(메뉴·로딩 등) Bark 자막도 함께 사라집니다.
 * - VM의 OnShowLine/OnHideLine을 받아 자막 한 줄(W_SubtitleLine)을 페이드 인/아웃합니다.
 *   무엇을 언제 보여줄지는 VM이 정하고, 이 위젯은 받은 텍스트·색을 채워 표시만 합니다.
 */
UCLASS()
class RETRIEVE_API URetrieveBarkWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UBarkViewModel* GetBarkViewModel() const { return BarkViewModel; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleShowLine();

	UFUNCTION()
	void HandleHideLine();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<URetrieveSubtitleLineWidget> SubtitleLine;

	/** 스피커 태그별 색상. WBP_Bark에서 DA_BarkStyle 할당. */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bark")
	TObjectPtr<UBarkStyleAsset> BarkStyle;

	UPROPERTY(Transient)
	TObjectPtr<UBarkViewModel> BarkViewModel;
};
