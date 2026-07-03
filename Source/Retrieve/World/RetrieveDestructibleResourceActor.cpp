#include "World/RetrieveDestructibleResourceActor.h"

#include "Collision/RetrieveCollisionChannels.h"
#include "Components/StaticMeshComponent.h"
#include "Components/World/RetrieveInteractionResponseComponent.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Logging/RetrieveLogChannels.h"
#include "Net/UnrealNetwork.h"

ARetrieveDestructibleResourceActor::ARetrieveDestructibleResourceActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	IntactMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("IntactMesh"));
	IntactMesh->SetupAttachment(SceneRoot);
	IntactMesh->SetVisibility(true);
	IntactMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	IntactMesh->SetCollisionObjectType(RetrieveCollisionChannels::Gatherable);

	FracturedMesh = CreateDefaultSubobject<UGeometryCollectionComponent>(TEXT("FracturedMesh"));
	FracturedMesh->SetupAttachment(SceneRoot);
	FracturedMesh->SetVisibility(false);
	FracturedMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FracturedMesh->SetSimulatePhysics(false);

	RewardComponent = CreateDefaultSubobject<URetrieveInteractionResponseComponent>(TEXT("RewardComponent"));
	RewardComponent->bAutoBindInteractionManager = false;
	RewardComponent->bDestroyOwnerOnApplied = false;
}

void ARetrieveDestructibleResourceActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARetrieveDestructibleResourceActor, bBroken);
}

bool ARetrieveDestructibleResourceActor::ReceiveRetrieveAttackHit_Implementation(
	AActor* Attacker,
	const FHitResult& HitResult,
	FGameplayTag AttackTypeTag,
	FGameplayTag ElementTag)
{
	if (!HasAuthority() || bBroken || !IsValid(Attacker))
	{
		return false;
	}

	++CurrentHitCount;

	UE_LOG(LogRetrieveWorld, Verbose, TEXT("[Resource] Hit Resource=%s Attacker=%s Count=%d/%d"),
		*GetName(), *GetNameSafe(Attacker), CurrentHitCount, RequiredHitCount);

	if (CurrentHitCount >= RequiredHitCount)
	{
		BreakResource(Attacker, HitResult.ImpactPoint);
	}
	else
	{
		MulticastPlayHitFeedback(HitResult.ImpactPoint, HitResult.ImpactNormal, CurrentHitCount);
	}

	return true;
}

void ARetrieveDestructibleResourceActor::BreakResource(AActor* Attacker, const FVector& ImpactPoint)
{
	if (!HasAuthority() || bBroken)
	{
		UE_LOG(LogRetrieveWorld, Verbose, TEXT("[Resource] Ignored duplicate break Resource=%s"), *GetName());
		return;
	}

	bBroken = true;
	IntactMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (RewardComponent)
	{
		RewardComponent->HandleInteractionApplied(Attacker);
	}

	UE_LOG(LogRetrieveWorld, Log, TEXT("[Resource] Broken Resource=%s"), *GetName());

	MulticastPlayBreak(ImpactPoint);
	ForceNetUpdate();
	SetLifeSpan(BrokenLifeSpan);
}

void ARetrieveDestructibleResourceActor::ApplyBrokenVisual(const FVector& ImpactPoint)
{
	if (bBreakVisualPlayed)
	{
		return;
	}
	bBreakVisualPlayed = true;

	IntactMesh->SetVisibility(false, true);
	IntactMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	FracturedMesh->SetVisibility(true, true);
	FracturedMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	FracturedMesh->SetSimulatePhysics(true);
	FracturedMesh->ApplyExternalStrain(INDEX_NONE, ImpactPoint, BreakRadius, 1, 1.0f, BreakStrain);

	// ApplyExternalStrain은 클러스터 결합을 끊을 뿐 힘을 주지 않으므로, 별도로 바깥 방향 임펄스를 줘서
	// 조각이 실제로 흩어지게 한다. bVelChange=true로 질량에 상관없이 일정한 튀는 정도를 유지한다.
	FracturedMesh->AddRadialImpulse(ImpactPoint, BreakRadius, BreakImpulseStrength, ERadialImpulseFalloff::RIF_Linear, /*bVelChange=*/true);

	PlayBreakFeedback(ImpactPoint);
}

void ARetrieveDestructibleResourceActor::OnRep_Broken()
{
	if (bBroken)
	{
		ApplyBrokenVisual(GetActorLocation());
	}
}

void ARetrieveDestructibleResourceActor::MulticastPlayHitFeedback_Implementation(FVector_NetQuantize ImpactPoint, FVector_NetQuantizeNormal ImpactNormal, int32 HitCount)
{
	const float HitProgress = RequiredHitCount > 0
		? static_cast<float>(HitCount) / static_cast<float>(RequiredHitCount)
		: 0.0f;

	PlayHitFeedback(ImpactPoint, ImpactNormal, HitCount, HitProgress);
}

void ARetrieveDestructibleResourceActor::MulticastPlayBreak_Implementation(FVector_NetQuantize ImpactPoint)
{
	ApplyBrokenVisual(ImpactPoint);
}
