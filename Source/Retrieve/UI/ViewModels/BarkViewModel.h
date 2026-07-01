#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "Components/SlateWrapperTypes.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "BarkViewModel.generated.h"

class UBarkStyleAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBarkLineEvent);

/**
 * Bark 자막을 실제로 화면에 띄우고 내리는 ViewModel.
 * - 들어온 대사를 FIFO 큐(최대 MaxQueued개)에 쌓고, 하나씩 [표시 → 유지 → 숨김 → 다음] 순서로 재생합니다.
 * - 시네마틱/대화 중에는 억제합니다(새 대사를 띄우지 않고, 진행 중이던 대사도 숨김)
 * - W_Bark가 생성 및 초기화(self-wire)하며, HUD가 다시 만들어질 때마다 새로 생성됩니다.
 */
UCLASS()
class RETRIEVE_API UBarkViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void Initialize(UWorld* InWorld, UBarkStyleAsset* InStyle);
	void Shutdown();

	// ---- 위젯이 자막을 띄우고/내리도록 구동하는 이벤트 (W_Bark가 바인딩) ----
	
	/** 새 대사를 띄울 때 발생. W_Bark가 받아 자막에 텍스트와 색을 채우고 화면에 표시합니다. */
	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Bark")
	FBarkLineEvent OnShowLine;

	/** 표시 시간이 끝나 대사를 내릴 때 발생. W_Bark가 받아 자막을 숨깁니다. */
	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Bark")
	FBarkLineEvent OnHideLine;

	// ---- MVVM FieldNotify getter ----
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Bark")
	FText GetSpeakerName() const { return SpeakerName; }

	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Bark")
	FText GetLineText() const { return LineText; }

	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Bark")
	FLinearColor GetNameColor() const { return NameColor; }

	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Bark")
	FLinearColor GetAccentColor() const { return AccentColor; }

	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Bark")
	bool GetIsLineVisible() const { return bLineVisible; }

	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Bark")
	ESlateVisibility GetSlateVisibility() const
	{
		return bLineVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	}

	/**
	 * 대사를 내리라고 알린 뒤 다음 대사로 넘어가기까지 기다리는 시간(초).
	 * 페이드아웃 애니메이션이 있으면 그 길이에 맞춥니다. 페이드를 쓰지 않으면 0으로 둡니다.
	 */
	UPROPERTY(EditAnywhere, Category = "Retrieve|Bark")
	float FadeOutSeconds = 0.0f;

	/** 시네마틱·대화가 끝난 뒤 곧바로 재개하지 않는 유예 시간(초). */
	UPROPERTY(EditAnywhere, Category = "Retrieve|Bark")
	float GraceSeconds = 2.5f;

	/** 대기 큐에 담을 수 있는 최대 대사 수. 넘치면 가장 오래된 대사부터 버립니다(FIFO). */
	UPROPERTY(EditAnywhere, Category = "Retrieve|Bark")
	int32 MaxQueued = 5;

protected:
	void HandleBarkRequested(FGameplayTag Channel, const FRetrieveBarkPayload& Payload);
	void HandleCinematicChanged(FGameplayTag Channel, const FRetrieveCinematicStatePayload& Payload);
	void HandleDialogueChanged(FGameplayTag Channel, const FRetrieveDialogueChangedPayload& Payload);

	void Enqueue(const FRetrieveBarkPayload& Payload);
	void PlayNext();
	void OnSuppressionChanged();

	// ---- FTSTicker 원샷 콜백. 지정한 시간 뒤 딱 한 번 실행되고, false를 반환해 스스로 종료됨 ----
	bool OnHoldExpired(float);
	bool OnFadeOutDone(float);
	bool OnGraceElapsed(float);
	void ClearActiveTicker();

	bool IsSuppressed() const { return bCinematicActive || bDialogueActive; }

private:
	TWeakObjectPtr<UWorld> WorldPtr;

	UPROPERTY(Transient)
	TObjectPtr<UBarkStyleAsset> Style;

	TArray<FRetrieveBarkPayload> Queue;
	bool bPlaying = false;
	bool bCinematicActive = false;
	bool bDialogueActive = false;

	FText SpeakerName;
	FText LineText;
	FLinearColor NameColor = FLinearColor::White;
	FLinearColor AccentColor = FLinearColor::White;
	bool bLineVisible = false;

	FGameplayMessageListenerHandle BarkHandle;
	FGameplayMessageListenerHandle CinematicHandle;
	FGameplayMessageListenerHandle DialogueHandle;

	FTSTicker::FDelegateHandle ActiveTicker;
};
