#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Math/Color.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "RetrieveBuffSlotWidget.generated.h"

class UImage;
class UTextBlock;

/**
 * 버프/디버프 단일 슬롯 위젯.
 * 자체 타이머 없음 — URetrieveBuffBarWidget이 일괄 구동한다.
 * WBP_BuffSlot의 부모 클래스로 설정한다.
 */
UCLASS()
class RETRIEVE_API URetrieveBuffSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetBuff(const FRetrieveUIBuffPayload& Payload);
	void UpdateDuration(float Remaining);
	void UpdateStack(int32 Count);
	void ClearBuff();

	bool IsActive() const { return bActive; }
	FGameplayTag GetActiveBuffId() const { return ActiveBuffId; }

private:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> IMG_Icon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TXT_Duration;

	/** 지속시간 비율을 표시하는 HPBar 위젯 (HUD_FantasyWarrior_HPBar_01_C). 없어도 동작함 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UUserWidget> DurationBar;

	/** 스택 수 표시 (×2, ×3 …). bIsStackable 버프에서만 사용. 없어도 동작함 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TXT_StackCount;

	bool bActive = false;
	FGameplayTag ActiveBuffId;
	float InitialDuration = 0.f;

	static FLinearColor ResolveStatusColor(bool bIsDebuff, const FLinearColor& DataColor);
	static FText BuildTooltipText(const FRetrieveUIBuffPayload& Payload);
	static void SetBarFillAmount(UUserWidget* Widget, float Amount);
	static void SetBarFillColor(UUserWidget* Widget, const FLinearColor& Color);
	static void CallBarUpdate(UUserWidget* Widget);
};
