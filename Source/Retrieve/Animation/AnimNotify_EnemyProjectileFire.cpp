#include "AnimNotify_EnemyProjectileFire.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayTags/RetrieveGameplayTags.h"

FString UAnimNotify_EnemyProjectileFire::GetNotifyName_Implementation() const
{
	return TEXT("EnemyProjectileFire");
}

void UAnimNotify_EnemyProjectileFire::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (!Owner)
	{
		return;
	}

	FGameplayEventData EventData;
	EventData.EventTag = RetrieveGameplayTags::GameplayEvent_Enemy_SpecialAttack_ProjectileFire;
	EventData.Instigator = Owner;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, EventData.EventTag, EventData);
	
}

