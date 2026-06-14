#pragma once

#include "CoreMinimal.h"
#include "UI/RetrieveGamePanelWidget.h"
#include "BonfireMenuWidget.generated.h"

/**
 * 모닥불 메뉴 위젯의 C++ 기반 클래스.
 * WBP_BonfireMenu의 부모 클래스로 지정한다.
 * PlayerController의 OpenExclusivePanel 흐름을 통해 열리므로
 * 커서·입력 모드 관리는 PlayerController에 완전히 위임된다.
 */
UCLASS(Abstract, Blueprintable)
class RETRIEVE_API UBonfireMenuWidget : public URetrieveGamePanelWidget
{
	GENERATED_BODY()

public:
	/** BonfireActor가 패널 생성 직후 주입하는 화톳불 식별자 */
	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Bonfire")
	FName BonfireId;

protected:
	virtual void NativeConstruct() override;
};
