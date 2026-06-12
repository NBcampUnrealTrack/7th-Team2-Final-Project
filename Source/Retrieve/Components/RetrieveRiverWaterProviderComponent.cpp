#include "Components/RetrieveRiverWaterProviderComponent.h"

#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SwimDetectionComponent.h"
#include "Engine/Engine.h"

URetrieveRiverWaterProviderComponent::URetrieveRiverWaterProviderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URetrieveRiverWaterProviderComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner) { return; }

	Spline = Owner->FindComponentByClass<USplineComponent>();
	if (!Spline) { return; }

	// 스플라인 포인트 바운드 → 폭/깊이 패딩으로 AABB.
	FBox Bounds(ForceInit);
	const int32 Num = Spline->GetNumberOfSplinePoints();
	for (int32 i = 0; i < Num; ++i)
	{
		Bounds += Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
	}
	Bounds = Bounds.ExpandBy(FVector(HalfWidth, HalfWidth, 0.f));
	Bounds.Min.Z -= Depth;
	Bounds.Max.Z += SurfaceMargin;

	WaterBox = NewObject<UBoxComponent>(Owner);
	WaterBox->RegisterComponent();
	WaterBox->AttachToComponent(Owner->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
	WaterBox->SetWorldRotation(FQuat::Identity);
	WaterBox->SetWorldLocation(Bounds.GetCenter());
	WaterBox->SetBoxExtent(Bounds.GetExtent());
	WaterBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	WaterBox->SetGenerateOverlapEvents(true);
	WaterBox->OnComponentBeginOverlap.AddDynamic(this, &URetrieveRiverWaterProviderComponent::HandleBeginOverlap);
	WaterBox->OnComponentEndOverlap.AddDynamic(this, &URetrieveRiverWaterProviderComponent::HandleEndOverlap);
}

void URetrieveRiverWaterProviderComponent::HandleBeginOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (USwimDetectionComponent* Swim =
		OtherActor ? OtherActor->FindComponentByClass<USwimDetectionComponent>() : nullptr)
	{
		Swim->NotifyEnterWaterRegion(this);
	}
}

void URetrieveRiverWaterProviderComponent::HandleEndOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32)
{
	if (USwimDetectionComponent* Swim =
		OtherActor ? OtherActor->FindComponentByClass<USwimDetectionComponent>() : nullptr)
	{
		Swim->NotifyExitWaterRegion();
	}
}

float URetrieveRiverWaterProviderComponent::GetWaterSurfaceZ_Implementation(const FVector& Location) const
{
	if (!Spline) { return Location.Z; }
	const float Key = Spline->FindInputKeyClosestToWorldLocation(Location);
	return Spline->GetLocationAtSplineInputKey(Key, ESplineCoordinateSpace::World).Z + SurfaceZOffset;
}

bool URetrieveRiverWaterProviderComponent::TryGetWaterColumn_Implementation(const FVector& Location, float& OutSurfaceZ) const
{
	if (!Spline) { return false; }

	const float Key = Spline->FindInputKeyClosestToWorldLocation(Location);
	const FTransform T = Spline->GetTransformAtSplineInputKey(Key, ESplineCoordinateSpace::World);
	const FVector Off = Location - T.GetLocation();

	// 스플라인 로컬 Y(폭) / Z(깊이) 축으로 채널 판정.
	const float Lateral = FMath::Abs(FVector::DotProduct(Off, T.GetUnitAxis(EAxis::Y)));
	const float Vertical = FVector::DotProduct(Off, T.GetUnitAxis(EAxis::Z));
	if (Lateral > HalfWidth || Vertical < -Depth || Vertical > SurfaceMargin)
	{
		// [임시 디버그] 채널 이탈 경계 확인용 — 해결 시 제거
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow,
				FString::Printf(TEXT("RIVER OUT: Lateral=%.1f(/%.0f) Vertical=%.1f(%.0f~%.0f)"),
					Lateral, HalfWidth, Vertical, -Depth, SurfaceMargin));
		}
		return false;
	}
	OutSurfaceZ = T.GetLocation().Z + SurfaceZOffset;
	return true;
}

FVector URetrieveRiverWaterProviderComponent::GetFlowVelocity_Implementation(const FVector& Location) const
{
	if (!Spline) { return FVector::ZeroVector; }
	const float Key = Spline->FindInputKeyClosestToWorldLocation(Location);
	return Spline->GetDirectionAtSplineInputKey(Key, ESplineCoordinateSpace::World) * FlowStrength;
}
