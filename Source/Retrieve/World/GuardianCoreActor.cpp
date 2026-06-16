#include "World/GuardianCoreActor.h"

#include "Components/World/RetrieveInteractionResponseComponent.h"
#include "Core/RetrieveGameState.h"
#include "Quest/QuestBranchComponent.h"


AGuardianCoreActor::AGuardianCoreActor()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	InteractionResponse = CreateDefaultSubobject<URetrieveInteractionResponseComponent>(TEXT("InteractionResponse"));
}

void AGuardianCoreActor::BeginPlay()
{
	Super::BeginPlay();

	if (InteractionResponse)
	{
		InteractionResponse->OnApplied.AddDynamic(this, &AGuardianCoreActor::HandleCoreInteracted);
	}
}

void AGuardianCoreActor::HandleCoreInteracted(AActor* InteractionInstigator)
{
	if (!HasAuthority())
	{
		return;
	}

	UQuestBranchComponent* Quest = nullptr;
	if (const UWorld* World = GetWorld())
	{
		if (const ARetrieveGameState* GS = World->GetGameState<ARetrieveGameState>())
		{
			Quest = GS->GetQuestBranchComponent();
		}
	}

	if (!Quest)
	{
		return;
	}

	Quest->CompleteStep(GuardianDefeatedStep); // 전제조건이 충족된 경우에만 성공
	if (Quest->IsStepCompleted(GuardianDefeatedStep))
	{
		UE_LOG(LogTemp, Warning, TEXT("[GuardianCore] %s: '%s' 완료, 코어 획득"), *GetName(), *GuardianDefeatedStep.ToString());
		Destroy();
	}
}
