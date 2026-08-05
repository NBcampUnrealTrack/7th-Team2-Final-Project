#include "UI/HUD/RetrieveQuestTrackerWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Subsystems/RetrieveObjectiveMarkerSubsystem.h"
#include "TimerManager.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "MVVMSubsystem.h"
#include "View/MVVMView.h"

#include "Core/RetrieveGameState.h"
#include "Player/RetrievePlayerController.h"
#include "UI/ViewModels/HUDViewModel.h"
#include "UI/ViewModels/QuestTrackerViewModel.h"

void URetrieveQuestTrackerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ARetrievePlayerController* PlayerController = Cast<ARetrievePlayerController>(GetOwningPlayer());
	if (!PlayerController)
	{
		return;
	}

	UHUDViewModel* HUDVM = PlayerController->GetHUDViewModel();
	if (!HUDVM)
	{
		return;
	}

	UQuestTrackerViewModel* TrackerVM = HUDVM->GetQuestTracker();
	if (!TrackerVM)
	{
		return;
	}

	if (UMVVMSubsystem* MVVM = GEngine ? GEngine->GetEngineSubsystem<UMVVMSubsystem>() : nullptr)
	{
		if (UMVVMView* View = MVVM->GetViewFromUserWidget(this))
		{
			View->SetViewModel(TEXT("QuestTracker"), TrackerVM);
		}
	}

	if (ARetrieveGameState* GS = GetWorld() ? GetWorld()->GetGameState<ARetrieveGameState>() : nullptr)
	{
		TrackerVM->InitializeFromGameState(GS);
	}

	// 목표 전환/재확인 순간에 강조 연출을 재생하기 위한 구독.
	if (UWorld* World = GetWorld())
	{
		UGameplayMessageSubsystem& Messaging = UGameplayMessageSubsystem::Get(World);

		StepChangedHandle = Messaging.RegisterListener<FRetrieveQuestStepPayload>(
			RetrieveGameplayTags::Channel_Quest_StepChanged,
			this, &URetrieveQuestTrackerWidget::HandleStepChanged);

		ReminderHandle = Messaging.RegisterListener<FRetrieveObjectiveReminderPayload>(
			RetrieveGameplayTags::Channel_UI_ObjectiveReminder,
			this, &URetrieveQuestTrackerWidget::HandleObjectiveReminder);

		// 의뢰 줄은 플레이어 이동에 따라 대상이 바뀌므로 주기적으로 갱신한다.
		World->GetTimerManager().SetTimer(
			SideQuestTimerHandle,
			FTimerDelegate::CreateUObject(this, &URetrieveQuestTrackerWidget::UpdateSideQuestLine),
			FMath::Max(SideQuestUpdateInterval, 0.05f), true);
		UpdateSideQuestLine();
	}
}

void URetrieveQuestTrackerWidget::UpdateSideQuestLine()
{
	if (!Box_SideQuest && !Text_SideQuestTitle && !Text_SideQuestObjective)
	{
		return; // WBP에 의뢰 줄을 두지 않았다면 아무것도 하지 않는다.
	}

	const UWorld* World = GetWorld();
	const URetrieveObjectiveMarkerSubsystem* MarkerSub =
		World ? World->GetSubsystem<URetrieveObjectiveMarkerSubsystem>() : nullptr;
	const APawn* Pawn = GetOwningPlayerPawn();

	auto HideSideQuest = [this]()
	{
		ShownSideQuestMarkerId = NAME_None;
		if (Box_SideQuest)
		{
			Box_SideQuest->SetVisibility(ESlateVisibility::Collapsed);
		}
	};

	if (!MarkerSub || !Pawn)
	{
		HideSideQuest();
		return;
	}

	const FVector PlayerLocation = Pawn->GetActorLocation();
	// 트래커는 "지금 가까이 있는" 의뢰만 — 멀어지면 줄이 사라진다.
	TArray<const FRetrieveObjectiveMarker*> Accepted;
	MarkerSub->GetAcceptedQuestMarkers(PlayerLocation, Accepted, /*bRequireCurrentProximity=*/true);

	if (Accepted.Num() == 0)
	{
		HideSideQuest();
		return;
	}

	// 가장 가까운 후보. 단, 지금 표시 중인 의뢰가 아직 유효하면 웬만해선 유지한다.
	const FRetrieveObjectiveMarker* Best = Accepted[0];
	if (!ShownSideQuestMarkerId.IsNone())
	{
		if (const FRetrieveObjectiveMarker* Current = MarkerSub->FindMarkerById(ShownSideQuestMarkerId))
		{
			const float CurrentDistSq = FVector::DistSquared(PlayerLocation, Current->State.WorldLocation);
			const float BestDistSq = FVector::DistSquared(PlayerLocation, Best->State.WorldLocation);
			const float Threshold = FMath::Square(FMath::Clamp(SideQuestSwitchRatio, 0.1f, 1.0f));

			// 새 후보가 "눈에 띄게" 가깝지 않으면 교체하지 않는다(줄이 깜빡이는 것을 막는다).
			if (BestDistSq > CurrentDistSq * Threshold)
			{
				Best = Current;
			}
		}
	}

	ShownSideQuestMarkerId = Best->MarkerId;

	if (Box_SideQuest)
	{
		Box_SideQuest->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (Text_SideQuestTitle)
	{
		Text_SideQuestTitle->SetText(FText::Format(INVTEXT("{0}{1}"), SideQuestPrefix, Best->Label));
		Text_SideQuestTitle->SetColorAndOpacity(FSlateColor(SideQuestColor));
	}
	if (Text_SideQuestObjective)
	{
		const bool bHasObjective = !Best->State.ProgressText.IsEmptyOrWhitespace();
		Text_SideQuestObjective->SetText(Best->State.ProgressText);
		Text_SideQuestObjective->SetVisibility(
			bHasObjective ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void URetrieveQuestTrackerWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SideQuestTimerHandle);

		UGameplayMessageSubsystem& Messaging = UGameplayMessageSubsystem::Get(World);
		if (StepChangedHandle.IsValid())
		{
			Messaging.UnregisterListener(StepChangedHandle);
		}
		if (ReminderHandle.IsValid())
		{
			Messaging.UnregisterListener(ReminderHandle);
		}
	}
	StepChangedHandle = FGameplayMessageListenerHandle();
	ReminderHandle = FGameplayMessageListenerHandle();

	Super::NativeDestruct();
}

void URetrieveQuestTrackerWidget::HandleStepChanged(
	FGameplayTag /*Channel*/, const FRetrieveQuestStepPayload& /*Message*/)
{
	PlayHighlight();
}

void URetrieveQuestTrackerWidget::HandleObjectiveReminder(
	FGameplayTag /*Channel*/, const FRetrieveObjectiveReminderPayload& /*Message*/)
{
	PlayHighlight();
}

void URetrieveQuestTrackerWidget::PlayHighlight()
{
	if (HighlightSound)
	{
		UGameplayStatics::PlaySound2D(this, HighlightSound);
	}

	// 위젯 애니메이션은 생성 클래스가 들고 있다. 같은 이름이 없으면 조용히 넘어간다.
	if (const UWidgetBlueprintGeneratedClass* BPClass = Cast<UWidgetBlueprintGeneratedClass>(GetClass()))
	{
		if (!HighlightAnimationName.IsNone())
		{
			for (UWidgetAnimation* Animation : BPClass->Animations)
			{
				if (Animation && Animation->GetFName() == HighlightAnimationName)
				{
					PlayAnimation(Animation);
					break;
				}
			}
		}
	}

	OnObjectiveHighlighted();
}
