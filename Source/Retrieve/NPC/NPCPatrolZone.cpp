#include "NPC/NPCPatrolZone.h"

#include "Components/BoxComponent.h"

ANPCPatrolZone::ANPCPatrolZone()
{
	PrimaryActorTick.bCanEverTick = false;

	ZoneBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("ZoneBounds"));
	ZoneBounds->InitBoxExtent(FVector(1000.f, 1000.f, 200.f));
	ZoneBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ZoneBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	ZoneBounds->SetHiddenInGame(true);
	RootComponent = ZoneBounds;
}

FVector ANPCPatrolZone::GetZoneCenter() const
{
	return GetActorLocation();
}

float ANPCPatrolZone::GetZoneRadius() const
{
	return ZoneBounds ? static_cast<float>(ZoneBounds->Bounds.SphereRadius) : 500.f;
}

bool ANPCPatrolZone::ContainsPoint(const FVector& Point) const
{
	return ZoneBounds && ZoneBounds->Bounds.GetBox().IsInside(Point);
}
