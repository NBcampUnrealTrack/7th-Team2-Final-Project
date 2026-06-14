#include "Data/RetrieveMapIconDataAsset.h"
#include "Components/RetrieveMapIconComponent.h"
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
