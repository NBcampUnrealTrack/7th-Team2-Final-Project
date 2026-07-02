#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Subsystems/SystemMessageSubsystem.h"
#include "RetrieveSystemMessageWidget.generated.h"

class UTextBlock;
struct FRetrieveCinematicStatePayload;

/**
 * 우상단 텍스트 전용 시스템 메시지 위젯. 큐는 USystemMessageSubsystem이 갖고, 이 위젯은 꺼내 보여주기만 하는 얇은 뷰입니다.
 * 시네마틱일 때만 억제하며, 표시 중 시네마틱/위젯 파괴가 겹치면 현재 항목을 큐로 되돌립니다.
 */
UCLASS()
class RETRIEVE_API URetrieveSystemMessageWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** OnQueued 콜백, 새 항목이 오면 표시를 시도합니다. */
	void HandleQueued();

	void HandleCinematicChanged(FGameplayTag Channel, const FRetrieveCinematicStatePayload& Message);

	/** 억제 중이 아니고 표시 중 아니라면 큐에서 하나 꺼내 표시하고, 비었으면 숨깁니다. */
	void PumpNext();

	UFUNCTION()
	void HandleHoldExpired();

	UFUNCTION()
	void HandleHideFinished();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MessageText;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> ShowAnim;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> HideAnim;

private:
	TWeakObjectPtr<USystemMessageSubsystem> Subsystem;

	/** 현재 표시 중인 항목. 시네마틱/파괴 시 큐로 되돌립니다. 위젯이 가진 유일한 메시지 상태. */
	FSystemMessageEntry CurrentEntry;
	bool bHasCurrent = false;
	bool bShowing = false;
	bool bCinematicActive = false;

	FDelegateHandle QueuedHandle;
	FGameplayMessageListenerHandle CinematicHandle;
	FTimerHandle HoldTimer;
};
