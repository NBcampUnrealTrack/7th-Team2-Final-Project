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

	UWorld* World = GetWorld();
	ARetrieveGameState* GS = World ? World->GetGameState<ARetrieveGameState>() : nullptr;
	UQuestBranchComponent* Quest = GS ? GS->GetQuestBranchComponent() : nullptr;
	if (!Quest)
	{
		return;
	}

	Quest->CompleteStep(GuardianDefeatedStep); // 전제조건이 충족된 경우에만 성공
	if (!Quest->IsStepCompleted(GuardianDefeatedStep))
	{
		return;
	}

	GS->ApplyGuardianCoreEmpowerment(GuardianDefeatedStep, GS->GetHostPawn());
	UE_LOG(LogTemp, Warning, TEXT("[GuardianCore] %s: '%s' 완료 + 강화, 코어 획득"), *GetName(), *GuardianDefeatedStep.ToString());
	Destroy();
}
