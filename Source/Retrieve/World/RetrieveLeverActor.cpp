#include "World/RetrieveLeverActor.h"

#include "Components/World/RetrieveInteractionResponseComponent.h"
#include "Net/UnrealNetwork.h"

ARetrieveLeverActor::ARetrieveLeverActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	InteractionResponse = CreateDefaultSubobject<URetrieveInteractionResponseComponent>(TEXT("InteractionResponse"));
}

void ARetrieveLeverActor::BeginPlay()
{
	Super::BeginPlay();

	bActivated = bStartActivated;
	ApplyLeverState(/*bInstant=*/true);

	if (InteractionResponse)
	{
		InteractionResponse->OnApplied.AddDynamic(this, &ARetrieveLeverActor::HandleInteracted);
	}
}

void ARetrieveLeverActor::HandleInteracted(AActor* /*InteractionInstigator*/)
{
	if (!HasAuthority())
	{
		return;
	}
	// 래치 버튼: 이미 켜졌으면 무시.
	if (bActivated && !bToggle)
	{
		return;
	}

	bActivated = !bActivated;
	ApplyLeverState(false);
}

void ARetrieveLeverActor::OnRep_bActivated()
{
	ApplyLeverState(false);
}

void ARetrieveLeverActor::ApplyLeverState(bool bInstant)
{
	OnLeverStateChangedBP(bActivated, bInstant);
	OnLeverChanged.Broadcast(this);
}

void ARetrieveLeverActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARetrieveLeverActor, bActivated);
}
