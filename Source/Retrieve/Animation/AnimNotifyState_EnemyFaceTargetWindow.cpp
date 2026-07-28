#include "Animation/AnimNotifyState_EnemyFaceTargetWindow.h"

#include "Character/RetrieveEnemyCharacter.h"
#include "Components/Enemy/EnemyCombatComponent.h"

FString UAnimNotifyState_EnemyFaceTargetWindow::GetNotifyName_Implementation() const
{
	return TEXT("EnemyFaceTargetWindow");
}

void UAnimNotifyState_EnemyFaceTargetWindow::NotifyTick(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (!MeshComp)
	{
		return;
	}

	ARetrieveEnemyCharacter* Enemy = Cast<ARetrieveEnemyCharacter>(MeshComp->GetOwner());
	if (!Enemy)
	{
		return;
	}

	UEnemyCombatComponent* EnemyCombatComponent = Enemy->GetComponentByClass<UEnemyCombatComponent>();
	if (!EnemyCombatComponent)
	{
		return;
	}

	EnemyCombatComponent->FaceFocusTarget(FrameDeltaTime, InterpSpeed, bYawOnly);
}
