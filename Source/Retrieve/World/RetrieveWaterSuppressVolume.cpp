#include "World/RetrieveWaterSuppressVolume.h"

#include "Components/BoxComponent.h"
#include "Components/Water/SwimDetectionComponent.h"

ARetrieveWaterSuppressVolume::ARetrieveWaterSuppressVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	SetRootComponent(Box);
	Box->SetBoxExtent(FVector(200.f));
	Box->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Box->SetGenerateOverlapEvents(true);
	Box->OnComponentBeginOverlap.AddDynamic(this, &ARetrieveWaterSuppressVolume::HandleBeginOverlap);
	Box->OnComponentEndOverlap.AddDynamic(this, &ARetrieveWaterSuppressVolume::HandleEndOverlap);
}

void ARetrieveWaterSuppressVolume::HandleBeginOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (USwimDetectionComponent* Swim =
		OtherActor ? OtherActor->FindComponentByClass<USwimDetectionComponent>() : nullptr)
	{
		Swim->ChangeWaterSuppress(1);
	}
}

void ARetrieveWaterSuppressVolume::HandleEndOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32)
{
	if (USwimDetectionComponent* Swim =
		OtherActor ? OtherActor->FindComponentByClass<USwimDetectionComponent>() : nullptr)
	{
		Swim->ChangeWaterSuppress(-1);
	}
}
