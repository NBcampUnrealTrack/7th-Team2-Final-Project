#include "Components/RetrieveWaterProviderComponent.h"

#include "Components/BoxComponent.h"
#include "Components/SwimDetectionComponent.h"

URetrieveWaterProviderComponent::URetrieveWaterProviderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URetrieveWaterProviderComponent::BeginPlay()
{
	Super::BeginPlay();

	if (WaterTriggerBox)
	{
		WaterTriggerBox->OnComponentBeginOverlap.AddDynamic(this, &URetrieveWaterProviderComponent::HandleBeginOverlap);
		WaterTriggerBox->OnComponentEndOverlap.AddDynamic(this, &URetrieveWaterProviderComponent::HandleEndOverlap);
	}
}

void URetrieveWaterProviderComponent::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (USwimDetectionComponent* SwimComp =
		OtherActor ? OtherActor->FindComponentByClass<USwimDetectionComponent>() : nullptr)
	{
		SwimComp->NotifyEnterWaterRegion(this); // this = IRetrieveWaterProvider
	}
}

void URetrieveWaterProviderComponent::HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (USwimDetectionComponent* SwimComp =
		OtherActor ? OtherActor->FindComponentByClass<USwimDetectionComponent>() : nullptr)
	{
		SwimComp->NotifyExitWaterRegion();
	}
}

float URetrieveWaterProviderComponent::GetWaterSurfaceZ_Implementation(const FVector& Location) const
{
	if (WaterTriggerBox)
	{
		// 박스 윗면 = 수면 Z
		return WaterTriggerBox->GetComponentLocation().Z + WaterTriggerBox->GetScaledBoxExtent().Z;
	}
	return Location.Z; // 박스 미지정 시 깊이차 0 → 수영 진입 안 함(안전)
}

FVector URetrieveWaterProviderComponent::GetFlowVelocity_Implementation(const FVector& Location) const
{
	return FlowVelocity;
}
