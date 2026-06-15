#include "Components/RetrieveCameraWaterProbeComponent.h"

#include "Components/RetrieveRiverWaterProviderComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	const FName CameraWaterProbeTag(TEXT("CameraWaterProbe"));

	bool IsRiverWaterProvider(const UObject* WaterObject)
	{
		return WaterObject && WaterObject->IsA<URetrieveRiverWaterProviderComponent>();
	}
}

URetrieveCameraWaterProbeComponent::URetrieveCameraWaterProbeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	InitSphereRadius(12.f);
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionObjectType(ECC_WorldDynamic);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap);
	SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	SetGenerateOverlapEvents(true);
	ComponentTags.AddUnique(CameraWaterProbeTag);
}

void URetrieveCameraWaterProbeComponent::BeginPlay()
{
	Super::BeginPlay();

	OnComponentBeginOverlap.AddUniqueDynamic(this, &URetrieveCameraWaterProbeComponent::HandleBeginOverlap);
	OnComponentEndOverlap.AddUniqueDynamic(this, &URetrieveCameraWaterProbeComponent::HandleEndOverlap);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &URetrieveCameraWaterProbeComponent::CheckInitialWaterOverlap);
	}
}

void URetrieveCameraWaterProbeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	OnComponentBeginOverlap.RemoveDynamic(this, &URetrieveCameraWaterProbeComponent::HandleBeginOverlap);
	OnComponentEndOverlap.RemoveDynamic(this, &URetrieveCameraWaterProbeComponent::HandleEndOverlap);

	CandidateWaters.Reset();
	CurrentWater = TScriptInterface<IRetrieveWaterProvider>();

	Super::EndPlay(EndPlayReason);
}

void URetrieveCameraWaterProbeComponent::HandleBeginOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	RegisterWaterProvidersFromActor(OtherActor);
}

void URetrieveCameraWaterProbeComponent::HandleEndOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32)
{
	UnregisterWaterProvidersFromActor(OtherActor);
}

void URetrieveCameraWaterProbeComponent::CheckInitialWaterOverlap()
{
	TArray<AActor*> OverlappingActors;
	GetOverlappingActors(OverlappingActors);
	for (const AActor* Actor : OverlappingActors)
	{
		RegisterWaterProvidersFromActor(Actor);
	}
}

void URetrieveCameraWaterProbeComponent::RegisterWaterProvidersFromActor(const AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	for (UActorComponent* Component : Actor->GetComponents())
	{
		if (Component && Component->Implements<URetrieveWaterProvider>())
		{
			RegisterWaterProvider(TScriptInterface<IRetrieveWaterProvider>(Component));
		}
	}
}

void URetrieveCameraWaterProbeComponent::UnregisterWaterProvidersFromActor(const AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	for (UActorComponent* Component : Actor->GetComponents())
	{
		if (Component && Component->Implements<URetrieveWaterProvider>())
		{
			UnregisterWaterProvider(Component);
		}
	}
}

void URetrieveCameraWaterProbeComponent::RegisterWaterProvider(const TScriptInterface<IRetrieveWaterProvider>& InWater)
{
	UObject* WaterObject = InWater.GetObject();
	if (!WaterObject)
	{
		return;
	}

	for (const TScriptInterface<IRetrieveWaterProvider>& Candidate : CandidateWaters)
	{
		if (Candidate.GetObject() == WaterObject)
		{
			return;
		}
	}

	CandidateWaters.Add(InWater);
}

void URetrieveCameraWaterProbeComponent::UnregisterWaterProvider(UObject* WaterObject)
{
	if (!WaterObject)
	{
		return;
	}

	for (int32 Index = CandidateWaters.Num() - 1; Index >= 0; --Index)
	{
		if (CandidateWaters[Index].GetObject() == WaterObject)
		{
			CandidateWaters.RemoveAtSwap(Index);
		}
	}

	if (CurrentWater.GetObject() == WaterObject)
	{
		CurrentWater = TScriptInterface<IRetrieveWaterProvider>();
	}
}

bool URetrieveCameraWaterProbeComponent::ResolveCurrentWater(const FVector& QueryLocation, float& OutSurfaceZ)
{
	UObject* PreviousWater = CurrentWater.GetObject();

	for (int32 Index = CandidateWaters.Num() - 1; Index >= 0; --Index)
	{
		if (!CandidateWaters[Index].GetObject())
		{
			CandidateWaters.RemoveAtSwap(Index);
		}
	}

	auto TryUseCandidate = [&](const TScriptInterface<IRetrieveWaterProvider>& Candidate)
	{
		UObject* CandidateObject = Candidate.GetObject();
		if (CandidateObject &&
			IRetrieveWaterProvider::Execute_TryGetWaterColumn(CandidateObject, QueryLocation, OutSurfaceZ))
		{
			CurrentWater = Candidate;
			return true;
		}
		return false;
	};

	if (PreviousWater && !IsRiverWaterProvider(PreviousWater))
	{
		for (const TScriptInterface<IRetrieveWaterProvider>& Candidate : CandidateWaters)
		{
			if (Candidate.GetObject() == PreviousWater && TryUseCandidate(Candidate))
			{
				return true;
			}
		}
	}

	for (const TScriptInterface<IRetrieveWaterProvider>& Candidate : CandidateWaters)
	{
		if (!IsRiverWaterProvider(Candidate.GetObject()) && TryUseCandidate(Candidate))
		{
			return true;
		}
	}

	if (PreviousWater && IsRiverWaterProvider(PreviousWater))
	{
		for (const TScriptInterface<IRetrieveWaterProvider>& Candidate : CandidateWaters)
		{
			if (Candidate.GetObject() == PreviousWater && TryUseCandidate(Candidate))
			{
				return true;
			}
		}
	}

	for (const TScriptInterface<IRetrieveWaterProvider>& Candidate : CandidateWaters)
	{
		if (IsRiverWaterProvider(Candidate.GetObject()) && TryUseCandidate(Candidate))
		{
			return true;
		}
	}

	CurrentWater = TScriptInterface<IRetrieveWaterProvider>();
	return false;
}

