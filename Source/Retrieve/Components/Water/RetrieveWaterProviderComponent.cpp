#include "Components/Water/RetrieveWaterProviderComponent.h"

#include "Components/BoxComponent.h"
#include "Components/Water/RetrieveCameraWaterProbeComponent.h"
#include "Components/Water/SwimDetectionComponent.h"

URetrieveWaterProviderComponent::URetrieveWaterProviderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URetrieveWaterProviderComponent::BeginPlay()
{
	Super::BeginPlay();

	// 미지정 시 태그로 액터의 BoxComponent 탐색 (박스 N개 중 지정).
	if (!WaterTriggerBox && GetOwner())
	{
		for (UActorComponent* Comp : GetOwner()->GetComponentsByTag(UBoxComponent::StaticClass(), WaterBoxTag))
		{
			WaterTriggerBox = Cast<UBoxComponent>(Comp);
			break;
		}
	}

	if (WaterTriggerBox)
	{
		WaterTriggerBox->OnComponentBeginOverlap.AddDynamic(this, &URetrieveWaterProviderComponent::HandleBeginOverlap);
		WaterTriggerBox->OnComponentEndOverlap.AddDynamic(this, &URetrieveWaterProviderComponent::HandleEndOverlap);
	}

	// 수면 = 태그된 메시 컴포넌트의 Z (박스와 분리 — 강 스플라인 메시 등 확장점).
	if (GetOwner())
	{
		for (UActorComponent* Comp : GetOwner()->GetComponentsByTag(USceneComponent::StaticClass(), WaterSurfaceTag))
		{
			ResolvedSurface = Cast<USceneComponent>(Comp);
			break;
		}
	}
}

void URetrieveWaterProviderComponent::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherComp && OtherComp->IsA<URetrieveCameraWaterProbeComponent>())
	{
		return;
	}

	if (USwimDetectionComponent* SwimComp =
		OtherActor ? OtherActor->FindComponentByClass<USwimDetectionComponent>() : nullptr)
	{
		SwimComp->NotifyEnterWaterRegion(this); // this = IRetrieveWaterProvider
	}
}

void URetrieveWaterProviderComponent::HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherComp && OtherComp->IsA<URetrieveCameraWaterProbeComponent>())
	{
		return;
	}

	if (USwimDetectionComponent* SwimComp =
		OtherActor ? OtherActor->FindComponentByClass<USwimDetectionComponent>() : nullptr)
	{
		SwimComp->NotifyExitWaterRegion(this);
	}
}

float URetrieveWaterProviderComponent::GetWaterSurfaceZ_Implementation(const FVector& Location) const
{
	// 수면 = 태그된 수면 메시 Z + 오프셋. (박스 윗면 아님 — 박스는 오버랩 전담)
	if (ResolvedSurface)
	{
		return ResolvedSurface->GetComponentLocation().Z + SurfaceZOffset;
	}
	if (const AActor* Owner = GetOwner()) // 폴백: 액터 평면 Z
	{
		return Owner->GetActorLocation().Z + SurfaceZOffset;
	}
	return Location.Z;
}

bool URetrieveWaterProviderComponent::TryGetWaterColumn_Implementation(const FVector& Location, float& OutSurfaceZ) const
{
	// 호수: 영역은 트리거 박스가 전담 → 항상 true + 수면 Z.
	OutSurfaceZ = GetWaterSurfaceZ_Implementation(Location);
	return true;
}

FRetrieveWaterPPMaterials URetrieveWaterProviderComponent::GetWaterPostProcessMaterials_Implementation() const
{
	return FRetrieveWaterPPMaterials(); // 호수는 자체 PP 볼륨이 담당 → FX 미적용
}

FVector URetrieveWaterProviderComponent::GetFlowVelocity_Implementation(const FVector& Location) const
{
	return FlowVelocity;
}
