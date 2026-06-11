#include "Animation/AnimNotifyState_EnemyMovementLock.h"

#include "Components/EnemyCombatComponent.h"

FString UAnimNotifyState_EnemyMovementLock::GetNotifyName_Implementation() const
{
	return TEXT("EnemyMovementLock");
}

void UAnimNotifyState_EnemyMovementLock::NotifyBegin(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	AActor* OwnerActor = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (!IsValid(OwnerActor))
	{
		return;
	}

	if (UEnemyCombatComponent* Combat = OwnerActor->FindComponentByClass<UEnemyCombatComponent>())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[EnemyMovementLock] Begin Owner=%s Combat=%s"),
			*GetNameSafe(OwnerActor),
			*GetNameSafe(Combat));
		Combat->SetMovementLockedByAttack(true);
	}
}

void UAnimNotifyState_EnemyMovementLock::NotifyEnd(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	AActor* OwnerActor = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (!IsValid(OwnerActor))
	{
		return;
	}

	if (UEnemyCombatComponent* Combat = OwnerActor->FindComponentByClass<UEnemyCombatComponent>())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[EnemyMovementLock] End Owner=%s Combat=%s"),
			*GetNameSafe(OwnerActor),
			*GetNameSafe(Combat));
		Combat->SetMovementLockedByAttack(false);
	}
}
