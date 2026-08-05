#include "World/RetrieveStoryTriggerVolume.h"

#include "Bark/BarkSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/World/RetrieveDialogueComponent.h"
#include "Components/World/RetrieveObjectiveAnchorComponent.h"
#include "Core/RetrieveGameState.h"
#include "Quest/QuestBranchComponent.h"
#include "Subsystems/RetrieveObjectiveMarkerSubsystem.h"
#include "Subsystems/SystemMessageSubsystem.h"

ARetrieveStoryTriggerVolume::ARetrieveStoryTriggerVolume()
{
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	SetRootComponent(Box);
	Box->SetBoxExtent(FVector(200.f));
	Box->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Box->SetGenerateOverlapEvents(true);
	Box->OnComponentBeginOverlap.AddDynamic(this, &ARetrieveStoryTriggerVolume::HandleBeginOverlap);
}

void ARetrieveStoryTriggerVolume::BeginPlay()
{
	Super::BeginPlay();

	// "동굴 빠져나가기", "마을에 진입하기"처럼 이 볼륨이 완료시키는 스텝은 곧 목표 지점이다.
	// 앵커를 레벨에 따로 배치하지 않아도 이 볼륨이 그대로 메인 퀘스트 마커가 되게 한다.
	// (마커는 추적 중인 스텝과 태그가 일치할 때만 표시되므로 항상 떠 있지는 않는다)
	if (!CompleteStepTag.IsValid() || !bCreateObjectiveAnchor)
	{
		return;
	}

	URetrieveObjectiveAnchorComponent* Anchor =
		NewObject<URetrieveObjectiveAnchorComponent>(this, TEXT("ObjectiveAnchor"));
	if (!Anchor)
	{
		return;
	}

	Anchor->ObjectiveStepTag = CompleteStepTag;
	Anchor->MarkerOffset = ObjectiveAnchorOffset;
	Anchor->RegisterComponent();

	// RegisterComponent가 컴포넌트 BeginPlay까지 돌려주는지는 등록 시점(액터 BeginPlay 도중)에
	// 따라 달라진다. 서브시스템 등록은 중복 호출이 안전(AddUnique)하므로 여기서 한 번 더 보장한다.
	if (UWorld* World = GetWorld())
	{
		if (URetrieveObjectiveMarkerSubsystem* MarkerSub = World->GetSubsystem<URetrieveObjectiveMarkerSubsystem>())
		{
			MarkerSub->RegisterAnchor(Anchor);
		}
	}
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

	// 시네마틱 중에는 발동하지 않는다 — 시퀀서가 폰을 움직여 볼륨을 밟는 경우 오발동/일회성(bFired) 소모 방지.
	// bFired를 건드리지 않으므로 시네마틱 종료 후 다시 진입하면 정상 발동한다.
	if (GS->GetCinematicState().IsActive())
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

	// 2) 퀘스트 스텝 완료 (원장이 선행 조건/중복을 검사). 성공했을 때만 일회성 볼륨을 소진시킴.
	if (CompleteStepTag.IsValid())
	{
		if (UQuestBranchComponent* Quest = GS->GetQuestBranchComponent())
		{
			if (Quest->CompleteStep(CompleteStepTag))
			{
				bDidSomething = true;
			}
		}
	}

	// 3) 시스템 메시지
	if (SystemMessageKeysOnEnter.Num() > 0)
	{
		if (USystemMessageSubsystem* SystemMessage = World->GetSubsystem<USystemMessageSubsystem>())
		{
			for (const FGameplayTag& Key : SystemMessageKeysOnEnter)
			{
				if (Key.IsValid())
				{
					SystemMessage->RequestMessagesByKey(Key);
					bDidSomething = true;
				}
			}
		}
	}

	// 4) Bark
	if (BarkKeysOnEnter.Num() > 0)
	{
		if (UBarkSubsystem* Bark = World->GetSubsystem<UBarkSubsystem>())
		{
			for (const FGameplayTag& Key : BarkKeysOnEnter)
			{
				if (Key.IsValid())
				{
					Bark->RequestBarkByKey(Key);
					bDidSomething = true;
				}
			}
		}
	}

	if (bDidSomething)
	{
		bFired = true;
	}
}
