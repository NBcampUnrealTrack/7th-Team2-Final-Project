#include "Data/RetrieveObjectiveAnchorDataAsset.h"

#include "Components/World/RetrieveObjectiveAnchorComponent.h"

#if WITH_EDITOR
#include "EngineUtils.h"
#include "Editor.h"
#include "GameFramework/Actor.h"
#include "Subsystems/RetrieveObjectiveMarkerSubsystem.h"
#include "World/RetrieveAreaClearQuestActor.h"
#include "World/RetrieveStoryTriggerVolume.h"
#endif

bool URetrieveObjectiveAnchorDataAsset::FindAnchorLocation(
	const FGameplayTag& StepTag, FVector& OutLocation) const
{
	if (!StepTag.IsValid())
	{
		return false;
	}

	for (const FRetrieveBakedObjectiveAnchor& Anchor : Anchors)
	{
		if (Anchor.ObjectiveStepTag.MatchesTagExact(StepTag))
		{
			OutLocation = Anchor.WorldLocation;
			return true;
		}
	}
	return false;
}

#if WITH_EDITOR
void URetrieveObjectiveAnchorDataAsset::RefreshFromLevel()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ObjectiveAnchorBake] 에디터 월드를 찾지 못했습니다."));
		return;
	}

	Anchors.Reset();

	// 태그별로 채택된 후보의 우선순위를 기억해 더 좋은 후보가 나오면 교체한다.
	TMap<FGameplayTag, int32> BestPriorityByTag;

	auto AddAnchor = [this, &BestPriorityByTag](const FGameplayTag& Tag, const FVector& Location,
		const AActor* Source, int32 Priority)
	{
		if (!Tag.IsValid())
		{
			return;
		}

		// 같은 스텝을 여러 액터가 주장할 수 있다(스토리 볼륨 + 구역클리어 액터 등).
		// 런타임 선택과 같은 서열을 적용해 "과제가 실제로 있는 곳"을 굽는다.
		if (const int32* ExistingPriority = BestPriorityByTag.Find(Tag))
		{
			if (Priority <= *ExistingPriority)
			{
				return;
			}
			Anchors.RemoveAll([&Tag](const FRetrieveBakedObjectiveAnchor& Existing)
			{
				return Existing.ObjectiveStepTag.MatchesTagExact(Tag);
			});
		}

		FRetrieveBakedObjectiveAnchor NewAnchor;
		NewAnchor.ObjectiveStepTag = Tag;
		NewAnchor.WorldLocation = Location;
		NewAnchor.SourceActorName = Source ? Source->GetName() : FString();
		Anchors.Add(MoveTemp(NewAnchor));
		BestPriorityByTag.Add(Tag, Priority);
	};

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const AActor* Actor = *It;
		if (!IsValid(Actor))
		{
			continue;
		}

		if (const URetrieveObjectiveAnchorComponent* Anchor =
			Actor->FindComponentByClass<URetrieveObjectiveAnchorComponent>())
		{
			AddAnchor(Anchor->ObjectiveStepTag, Actor->GetActorLocation() + Anchor->MarkerOffset, Actor,
				URetrieveObjectiveMarkerSubsystem::GetAnchorPriority(Anchor));
		}

		// 아래 둘은 런타임에 앵커를 자가 생성하므로 에디터에는 컴포넌트가 없다. 태그에서 직접 굽는다.
		if (const ARetrieveAreaClearQuestActor* AreaClear = Cast<ARetrieveAreaClearQuestActor>(Actor))
		{
			AddAnchor(AreaClear->GetObjectiveStepTag(),
				Actor->GetActorLocation() + FVector(0.0f, 0.0f, 150.0f), Actor, /*Priority=*/10);
		}

		if (const ARetrieveStoryTriggerVolume* Volume = Cast<ARetrieveStoryTriggerVolume>(Actor))
		{
			AddAnchor(Volume->GetObjectiveStepTag(),
				Actor->GetActorLocation() + Volume->GetObjectiveAnchorOffset(), Actor, /*Priority=*/-10);
		}
	}

	MarkPackageDirty();

	UE_LOG(LogTemp, Log, TEXT("[ObjectiveAnchorBake] 목표 지점 %d개를 구웠습니다. (로드되지 않은 지역은 제외됩니다)"),
		Anchors.Num());
	for (const FRetrieveBakedObjectiveAnchor& Anchor : Anchors)
	{
		UE_LOG(LogTemp, Log, TEXT("  %s <- %s"),
			*Anchor.ObjectiveStepTag.ToString(), *Anchor.SourceActorName);
	}
}
#endif
