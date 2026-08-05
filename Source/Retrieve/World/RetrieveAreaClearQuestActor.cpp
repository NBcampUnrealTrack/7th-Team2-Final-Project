#include "World/RetrieveAreaClearQuestActor.h"

#include "Components/SceneComponent.h"
#include "Components/World/RetrieveObjectiveAnchorComponent.h"
#include "Core/RetrieveGameState.h"
#include "Subsystems/RetrieveObjectiveMarkerSubsystem.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Messaging/GameplayMessages/RetrieveGameplayMessageTypes.h"
#include "Quest/QuestBranchComponent.h"

ARetrieveAreaClearQuestActor::ARetrieveAreaClearQuestActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void ARetrieveAreaClearQuestActor::BeginPlay()
{
	Super::BeginPlay();

	ClearedHandle = UGameplayMessageSubsystem::Get(this).RegisterListener<FSpawnGroupClearedPayload>(
		RetrieveGameplayTags::Channel_Enemy_SpawnGroupCleared,
		this, &ARetrieveAreaClearQuestActor::OnSpawnGroupCleared);

	// 이 액터가 완료시키는 메인 스텝("늑대 무리 처치" 등)의 위치를 마커에 알린다.
	// 스토리 볼륨과 같은 방식으로, 앵커를 따로 배치하지 않아도 목표 지점이 생긴다.
	if (CompleteStepTag.IsValid() && bCreateObjectiveAnchor)
	{
		if (URetrieveObjectiveAnchorComponent* Anchor =
			NewObject<URetrieveObjectiveAnchorComponent>(this, TEXT("ObjectiveAnchor")))
		{
			Anchor->ObjectiveStepTag = CompleteStepTag;
			Anchor->RegisterComponent();

			if (UWorld* World = GetWorld())
			{
				if (URetrieveObjectiveMarkerSubsystem* MarkerSub =
					World->GetSubsystem<URetrieveObjectiveMarkerSubsystem>())
				{
					MarkerSub->RegisterAnchor(Anchor);
				}
			}
		}
	}
}

void ARetrieveAreaClearQuestActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ClearedHandle.IsValid())
	{
		UGameplayMessageSubsystem::Get(this).UnregisterListener(ClearedHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void ARetrieveAreaClearQuestActor::OnSpawnGroupCleared(FGameplayTag Channel, const FSpawnGroupClearedPayload& Payload)
{
	// TODO(coop): 공유되는 내러티브에서는 비호스트 클라이언트 관전/로컬 처리로 확장
	if (!HasAuthority() || (bOnce && bFired))
	{
		return;
	}
	
	if (!SpawnGroupId.IsValid() || Payload.SpawnGroupId != SpawnGroupId)
	{
		return;
	}

	UWorld* World = GetWorld();
	ARetrieveGameState* GS = World ? World->GetGameState<ARetrieveGameState>() : nullptr;
	if (!GS)
	{
		return;
	}

	// 다른 트리거가 같은 스텝을 먼저 완료했어도 안전합니다.
	if (UQuestBranchComponent* Quest = GS->GetQuestBranchComponent())
	{
		Quest->CompleteStep(CompleteStepTag);
		bFired = true;
	}
}
