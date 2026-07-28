#include "World/RetrieveMinimapAreaVolume.h"

#include "Components/BoxComponent.h"
#include "Data/RetrieveMinimapAreaDataAsset.h"
#include "Engine/Texture2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Subsystems/RetrieveMapSubsystem.h"

ARetrieveMinimapAreaVolume::ARetrieveMinimapAreaVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	AreaBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("AreaBounds"));
	SetRootComponent(AreaBounds);
	AreaBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AreaBounds->SetGenerateOverlapEvents(false);
	AreaBounds->SetBoxExtent(FVector(1000.0f, 1000.0f, 500.0f));

	CaptureIncludeMeshPatterns = {
		TEXT("Floor"), TEXT("Ground"), TEXT("Platform"), TEXT("Bridge"),
		TEXT("Stair"), TEXT("Wall"), TEXT("Pillar"), TEXT("Column"),
		TEXT("Arch"), TEXT("Door"), TEXT("Gate"), TEXT("Balustrade"),
		TEXT("Beam")
	};
}

void ARetrieveMinimapAreaVolume::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		if (URetrieveMapSubsystem* MapSubsystem = World->GetSubsystem<URetrieveMapSubsystem>())
		{
			MapSubsystem->RegisterMinimapArea(this);
		}
	}
}

void ARetrieveMinimapAreaVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (URetrieveMapSubsystem* MapSubsystem = World->GetSubsystem<URetrieveMapSubsystem>())
		{
			MapSubsystem->UnregisterMinimapArea(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

bool ARetrieveMinimapAreaVolume::ContainsLocation(const FVector& WorldLocation) const
{
	if (!AreaBounds)
	{
		return false;
	}

	const FTransform ComponentTransform = AreaBounds->GetComponentTransform();
	const FVector LocalLocation = ComponentTransform.InverseTransformPosition(WorldLocation);
	const FVector Extent = AreaBounds->GetUnscaledBoxExtent();
	return FMath::Abs(LocalLocation.X) <= Extent.X &&
		FMath::Abs(LocalLocation.Y) <= Extent.Y &&
		FMath::Abs(LocalLocation.Z) <= Extent.Z;
}

FBox ARetrieveMinimapAreaVolume::GetAreaBox() const
{
	return AreaBounds ? AreaBounds->Bounds.GetBox() : FBox(ForceInit);
}

FRetrieveMinimapContext ARetrieveMinimapAreaVolume::BuildContext() const
{
	FRetrieveMinimapContext Context;
	Context.SourceArea = const_cast<ARetrieveMinimapAreaVolume*>(this);

	const FBox Bounds = GetAreaBox();
	const FVector Center = Bounds.IsValid ? Bounds.GetCenter() : GetActorLocation();
	const FVector Extent = Bounds.IsValid ? Bounds.GetExtent() : FVector(1.0f);
	const float SquareDiameter = FMath::Max(Extent.X, Extent.Y) * 2.0f;

	Context.MapCenter = FVector2D(Center.X, Center.Y);
	Context.MapExtentXY = FVector2D(
		FMath::Max(SquareDiameter, 1.0f),
		FMath::Max(SquareDiameter, 1.0f));
	Context.MinZ = Bounds.IsValid ? Bounds.Min.Z : -BIG_NUMBER;
	Context.MaxZ = Bounds.IsValid ? Bounds.Max.Z : BIG_NUMBER;

	if (!AreaData)
	{
		Context.DisplayMode = ERetrieveMinimapDisplayMode::Black;
		Context.bShowIcons = false;
		Context.bShowWaypoints = false;
		return Context;
	}

	Context.DisplayMode = AreaData->DisplayMode;
	Context.ViewWorldRadius = AreaData->ViewWorldRadius;
	Context.bShowIcons = AreaData->bShowIcons;
	Context.bShowWaypoints = AreaData->bShowWaypoints;
	Context.bShowPlayerMarker = AreaData->bShowPlayerMarker;

	if (AreaData->DisplayMode == ERetrieveMinimapDisplayMode::BakedTexture)
	{
		Context.Texture = AreaData->BakedTexture;
	}
	else if (AreaData->DisplayMode == ERetrieveMinimapDisplayMode::LiveRenderTarget)
	{
		Context.Texture = RuntimeTexture;
	}

	return Context;
}

bool ARetrieveMinimapAreaVolume::ShouldCaptureStaticMeshComponent(
	const UStaticMeshComponent* Component) const
{
	if (!IsValid(Component) || !IsValid(Component->GetStaticMesh()))
	{
		return false;
	}

	if (!bFilterSourceStaticMeshComponents || CaptureIncludeMeshPatterns.IsEmpty())
	{
		return true;
	}

	const FString MeshPath = Component->GetStaticMesh()->GetPathName();
	return CaptureIncludeMeshPatterns.ContainsByPredicate(
		[&MeshPath](const FString& Pattern)
		{
			return !Pattern.IsEmpty() && MeshPath.Contains(Pattern, ESearchCase::IgnoreCase);
		});
}
