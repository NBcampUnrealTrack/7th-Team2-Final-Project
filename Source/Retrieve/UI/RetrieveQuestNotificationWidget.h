#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Subsystems/QuestNotificationSubsystem.h"
#include "RetrieveQuestNotificationWidget.generated.h"

class UTextBlock;
class URetainerBox;
class UMaterialInstanceDynamic;

enum class EQuestRevealPhase : uint8
{
	Idle,
	Revealing,
	Hiding
};

/**
 * 상단 중앙 퀘스트 수락/완료 알림 토스트. 시네마틱 컷씬 + reveal 게이트일 때 억제되며, 대화 중에는 허용됩니다.
 */
UCLASS()
class RETRIEVE_API URetrieveQuestNotificationWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void HandleQueued();
	void HandleCinematicChanged(FGameplayTag Channel, const FRetrieveCinematicStatePayload& Message);
	void HandleRevealGate(FGameplayTag Channel, const FRetrieveRevealGatePayload& Message);

	/** 억제 중이 아니고 이미 표시 중이 아니면 다음 항목을 표시합니다; 큐가 비었으면 숨김. */
	void PumpNext();

	/** 타이머 해제, 애니메이션 정지, 진행 중이던 항목을 다시 큐에 넣고 숨김. 두가지 억제 경로가 공유합니다. */
	void SuppressAndRequeue();

	UFUNCTION()
	void HandleHoldExpired();

	UFUNCTION()
	void HandleHideFinished();

	FText ResolveHeaderText(EQuestNotificationKind Kind) const;
	void RefreshRevealMaterial();
	void ResetReveal(bool bVisible);
	void StartRevealIn();
	void StartRevealOut();
	void TickReveal(float DeltaTime);
	void SetRevealSweep(float Value);

	/** BP 마감 훅: 종류별로 색상 재조정 / 효과음 재생. ShowAnim 직전에 호출됩니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Retrieve|UI|Quest")
	void OnPresent(EQuestNotificationKind Kind, const FText& QuestName);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> HeaderText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> QuestNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URetainerBox> RevealRetainer;

	UPROPERTY(Transient, BlueprintReadOnly, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> ShowAnim;

	UPROPERTY(Transient, BlueprintReadOnly, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> HideAnim;

	UPROPERTY(EditAnywhere, Category = "Retrieve|UI|Quest")
	FText StartedHeader;

	UPROPERTY(EditAnywhere, Category = "Retrieve|UI|Quest")
	FText CompletedHeader;

	UPROPERTY(EditAnywhere, Category = "Retrieve|UI|Quest|Reveal")
	FName RevealSweepParameter = TEXT("Sweep");

	UPROPERTY(EditAnywhere, Category = "Retrieve|UI|Quest|Reveal")
	FName RevealDirectionParameter = TEXT("Direction");

	UPROPERTY(EditAnywhere, Category = "Retrieve|UI|Quest|Reveal")
	bool bUseDirectionForHide = false;

	UPROPERTY(EditAnywhere, Category = "Retrieve|UI|Quest|Reveal")
	float HiddenSweep = -0.25f;

	UPROPERTY(EditAnywhere, Category = "Retrieve|UI|Quest|Reveal")
	float VisibleSweep = 1.25f;

	UPROPERTY(EditAnywhere, Category = "Retrieve|UI|Quest|Reveal")
	float RevealInSeconds = 0.45f;

	UPROPERTY(EditAnywhere, Category = "Retrieve|UI|Quest|Reveal")
	float RevealOutSeconds = 0.45f;

private:
	TWeakObjectPtr<UQuestNotificationSubsystem> Subsystem;
	TObjectPtr<UMaterialInstanceDynamic> RevealMaterial;

	FQuestNotificationEntry CurrentEntry;
	bool bHasCurrent = false;
	bool bShowing = false;
	bool bCinematicActive = false;
	bool bRevealBlocked = false;
	EQuestRevealPhase RevealPhase = EQuestRevealPhase::Idle;
	float RevealElapsed = 0.f;

	FDelegateHandle QueuedHandle;
	FGameplayMessageListenerHandle CinematicHandle;
	FGameplayMessageListenerHandle RevealHandle;
	FTimerHandle HoldTimer;
};
