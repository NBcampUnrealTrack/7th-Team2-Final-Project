#include "Animation/AnimNotifyState_EnemyRadialKnockback.h"

#include "Combat/RetrieveKnockbackLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

FString UAnimNotifyState_EnemyRadialKnockback::GetNotifyName_Implementation() const
{
	return TEXT("EnemyRadialKnockback");
}

void UAnimNotifyState_EnemyRadialKnockback::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp)
	{
		return;
	}

	ElapsedTimeByMesh.FindOrAdd(MeshComp) = 0.f;

	if (bApplyOnBegin)
	{
		ApplyKnockback(MeshComp);
	}
}

void UAnimNotifyState_EnemyRadialKnockback::NotifyTick(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (!MeshComp)
	{
		return;
	}

	float& ElapsedTime = ElapsedTimeByMesh.FindOrAdd(MeshComp);
	ElapsedTime += FrameDeltaTime;

	if (ApplyInterval <= 0.f || ElapsedTime >= ApplyInterval)
	{
		ElapsedTime = 0.f;
		ApplyKnockback(MeshComp);
	}
}

void UAnimNotifyState_EnemyRadialKnockback::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (MeshComp)
	{
		ElapsedTimeByMesh.Remove(MeshComp);
	}
}

FVector UAnimNotifyState_EnemyRadialKnockback::ResolveCenter(USkeletalMeshComponent* MeshComp) const
{
	if (!MeshComp)
	{
		return FVector::ZeroVector;
	}

	if (!BoneName.IsNone())
	{
		const FTransform BoneTransform = MeshComp->GetSocketTransform(BoneName, RTS_World);
		return BoneTransform.TransformPosition(Offset);
	}

	if (const AActor* OwnerActor = MeshComp->GetOwner())
	{
		return OwnerActor->GetActorTransform().TransformPosition(Offset);
	}

	return MeshComp->GetComponentTransform().TransformPosition(Offset);
}

void UAnimNotifyState_EnemyRadialKnockback::ApplyKnockback(USkeletalMeshComponent* MeshComp) const
{
	if (!MeshComp || Radius <= 0.f)
	{
		return;
	}

	TArray<AActor*> IgnoreActors;
	if (AActor* OwnerActor = MeshComp->GetOwner())
	{
		IgnoreActors.Add(OwnerActor);
	}

	URetrieveKnockbackLibrary::ApplyRadialKnockback(
		MeshComp,
		ResolveCenter(MeshComp),
		Radius,
		KnockbackParams,
		IgnoreActors);
}
