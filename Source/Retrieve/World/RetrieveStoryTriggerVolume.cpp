#include "World/RetrieveStoryTriggerVolume.h"

#include "Components/BoxComponent.h"
#include "Components/World/RetrieveDialogueComponent.h"
#include "Core/RetrieveGameState.h"
#include "Quest/QuestBranchComponent.h"

ARetrieveStoryTriggerVolume::ARetrieveStoryTriggerVolume()
{
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	SetRootComponent(Box);
	Box->SetBoxExtent(FVector(200.f));
	Box->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Box->SetGenerateOverlapEvents(true);
	Box->OnComponentBeginOverlap.AddDynamic(this, &ARetrieveStoryTriggerVolume::HandleBeginOverlap);
}

void ARetrieveStoryTriggerVolume::HandleBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                                     UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                                                     bool bFromSweep, const FHitResult& Sweep)
{
	// 스토리/퀘스트 진행은 호스트 권한 전용.
	if (!HasAuthority() || (bOnce && bFired))
	{
		return;
	}

	UWorld* World = GetWorld();
	ARetrieveGameState* GS = World ? World->GetGameState<ARetrieveGameState>() : nullptr;
	if (!GS)
	{
		return;
	}

	// 호스트 폰이 진입한 경우에만 발동. Lumen과 마찬가지로 진행은 호스트를 따른다.
	// TODO(coop): 공유 내러티브 시 비호스트 클라이언트도 로컬에서 관전하도록 확장.
	APawn* OverlappingPawn = Cast<APawn>(OtherActor);
	if (!OverlappingPawn || OverlappingPawn != GS->GetHostPawn())
	{
		return;
	}

	bool bDidSomething = false;

	// 1) 대상 NPC 대화 자동 시작 ([F] 상호작용과 동일한 호스트 전용 경로).
	if (DialogueTarget)
	{
		if (URetrieveDialogueComponent* Dialogue = DialogueTarget->FindComponentByClass<URetrieveDialogueComponent>())
		{
			Dialogue->HandleInteract(OverlappingPawn);
			bDidSomething = true;
		}
	}

	// 2) 퀘스트 스텝 완료 (원장이 선행 조건/중복을 검사).
	if (CompleteStepTag.IsValid())
	{
		if (UQuestBranchComponent* Quest = GS->GetQuestBranchComponent())
		{
			Quest->CompleteStep(CompleteStepTag);
			bDidSomething = true;
		}
	}

	if (bDidSomething)
	{
		bFired = true;
	}
}
