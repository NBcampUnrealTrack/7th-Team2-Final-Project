#include "Data/RetrieveMapIconDataAsset.h"
#include "Components/World/RetrieveMapIconComponent.h"
#include "Components/ArrowComponent.h"
#include "World/RetrieveBonfireActor.h"
#include "EngineUtils.h"

#if WITH_EDITOR
#include "Editor.h"

void URetrieveMapIconDataAsset::RefreshFromLevel()
{
	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!EditorWorld)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MapIconDataAsset] 에디터 월드를 찾을 수 없습니다."));
		return;
	}

	Icons.Empty();

	for (TActorIterator<AActor> It(EditorWorld); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor)) { continue; }

		URetrieveMapIconComponent* IconComp =
			Actor->FindComponentByClass<URetrieveMapIconComponent>();
		if (!IsValid(IconComp)) { continue; }

		FRetrieveMapIconEntry Entry;
		Entry.WorldLocation = Actor->GetActorLocation();
		Entry.IconType      = IconComp->IconType;
		Entry.MapLabel      = IconComp->MapLabel;
		Entry.bShowLabel    = IconComp->bShowLabelOnWorldMap;

		if (const ARetrieveBonfireActor* Bonfire = Cast<ARetrieveBonfireActor>(Actor))
		{
			Entry.BonfireId = Bonfire->BonfireId;
			if (Entry.BonfireId.IsNone())
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[MapIconDataAsset] BonfireId is empty: %s"),
					*Actor->GetName());
			}

			// 에디터에서 활성으로 배치된 모닥불은 데이터에 활성 상태 + 도착 Transform을 굽는다.
			// → WP 스트리밍 여부와 무관하게 월드맵 활성 표시 + 빠른이동 가능.
			Entry.bStartActivated = Bonfire->IsActivated();
			Entry.ArrivalTransform = IsValid(Bonfire->ArrivalPoint)
				? Bonfire->ArrivalPoint->GetComponentTransform()
				: Bonfire->GetActorTransform();
		}

		Icons.Add(Entry);
	}

	MarkPackageDirty();

	UE_LOG(LogTemp, Log,
		TEXT("[MapIconDataAsset] RefreshFromLevel 완료 — 총 %d개 아이콘 등록"),
		Icons.Num());

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green,
			FString::Printf(TEXT("WorldMap 아이콘 %d개 등록 완료"), Icons.Num()));
	}
}
#endif
