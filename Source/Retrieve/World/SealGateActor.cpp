#include "World/SealGateActor.h"

#include "Components/World/RetrieveInteractionResponseComponent.h"
#include "Core/RetrieveGameState.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "Quest/QuestBranchComponent.h"

ASealGateActor::ASealGateActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	InteractionResponse = CreateDefaultSubobject<URetrieveInteractionResponseComponent>(TEXT("InteractionResponse"));

	RequiredStep = RetrieveGameplayTags::Quest_Step_SealUnlocked;
	OpenStep = RetrieveGameplayTags::Quest_Step_SealOpened;
}

void ASealGateActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASealGateActor, bOpened);
}

void ASealGateActor::BeginPlay()
{
	Super::BeginPlay();

	if (InteractionResponse)
	{
		InteractionResponse->OnApplied.AddDynamic(this, &ASealGateActor::HandleInteracted);
	}
}

void ASealGateActor::HandleInteracted(AActor* /*InteractionInstigator*/)
{
	if (!HasAuthority() || bOpened)
	{
		return;
	}

	UQuestBranchComponent* QuestBranchComponent = nullptr;
	if (const UWorld* World = GetWorld())
	{
		if (const ARetrieveGameState* GS = World->GetGameState<ARetrieveGameState>())
		{
			QuestBranchComponent = GS->GetQuestBranchComponent();
		}
	}
	if (!QuestBranchComponent)
	{
		return;
	}

	// 봉인이 해제될 때까지 (세 원소 강화 모두 완료) 개방 불가
	if (RequiredStep.IsValid() && !QuestBranchComponent->IsStepCompleted(RequiredStep))
	{
		UE_LOG(LogTemp, Log, TEXT("[SealGate] %s: '%s' 미완료, 봉인 해제 불가"), *GetName(), *RequiredStep.ToString());
		return;
	}

	bOpened = true;
	OnRep_bOpened();
	QuestBranchComponent->CompleteStep(OpenStep);

	UE_LOG(LogTemp, Log, TEXT("[SealGate] %s: 개방됨 → '%s' 완료"), *GetName(), *OpenStep.ToString());
}

void ASealGateActor::OnRep_bOpened()
{
	if (bOpened)
	{
		OnGateOpened.Broadcast();
	}
}
