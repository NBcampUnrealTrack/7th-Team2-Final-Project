#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"
#include "RetrieveGamePanelWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRetrievePanelCloseRequestedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRetrievePanelTabSwitchRequestedSignature, int32, TabIndex);

// 독립 패널 위젯의 공통 기반 클래스.
// 닫기·토글 키 입력을 처리하며, 패널별 내용은 하위 클래스에서 구현한다.
UCLASS(Abstract, Blueprintable)
class RETRIEVE_API URetrieveGamePanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	URetrieveGamePanelWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Panel")
	void RequestClose();

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Panel")
	FRetrievePanelCloseRequestedSignature OnCloseRequested;

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Panel")
	FRetrievePanelTabSwitchRequestedSignature OnTabSwitchRequested;

	// PlayerController가 패널 생성 시 주입하는 토글 단축키
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Panel")
	FKey ToggleKey;

protected:
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
};
