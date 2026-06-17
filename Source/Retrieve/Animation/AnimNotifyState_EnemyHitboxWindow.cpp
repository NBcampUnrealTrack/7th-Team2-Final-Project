#include "Animation/AnimNotifyState_EnemyHitboxWindow.h"
#include "Character/RetrieveEnemyCharacter.h"
#include "Components/Enemy/EnemyCombatComponent.h"
FString UAnimNotifyState_EnemyHitboxWindow::GetNotifyName_Implementation() const
{
	return TEXT("EnemyHitboxWindow");
}

void UAnimNotifyState_EnemyHitboxWindow::NotifyBegin(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	
	if (!MeshComp)
	{
		return;
	}

	if (!MeshComp->GetOwner())
	{
		return;
	}
	
	if (ARetrieveEnemyCharacter* Enemy = Cast<ARetrieveEnemyCharacter>(MeshComp->GetOwner()))
	{
		UEnemyCombatComponent* EnemyCombatComponent = Enemy->GetComponentByClass<UEnemyCombatComponent>();
		if (EnemyCombatComponent)
		{
			EnemyCombatComponent->ActivateHitbox(BoneName, Offset, Radius);
		}
	}
}

void UAnimNotifyState_EnemyHitboxWindow::NotifyEnd(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	
	if (!MeshComp)
	{
		return;
	}

	if (!MeshComp->GetOwner())
	{
		return;
	}
	
	if (ARetrieveEnemyCharacter* Enemy = Cast<ARetrieveEnemyCharacter>(MeshComp->GetOwner()))
	{
		UEnemyCombatComponent* EnemyCombatComponent = Enemy->GetComponentByClass<UEnemyCombatComponent>();
		if (EnemyCombatComponent)
		{
			EnemyCombatComponent->DeactivateHitbox();
		}
	}
}
