#include "Animation/AnimNotifyState_ActionLock.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"

UAnimNotifyState_ActionLock::UAnimNotifyState_ActionLock()
{
	BlockedAbilityTags.AddTag(RetrieveGameplayTags::Ability_Type_Attack);
	BlockedAbilityTags.AddTag(RetrieveGameplayTags::Ability_Player_Guard);
	BlockedAbilityTags.AddTag(RetrieveGameplayTags::Ability_Player_Parry);
	BlockedAbilityTags.AddTag(RetrieveGameplayTags::Ability_Player_Dash);
	BlockedAbilityTags.AddTag(RetrieveGameplayTags::Ability_Player_Blink);
	BlockedAbilityTags.AddTag(RetrieveGameplayTags::Ability_Player_Burst);
}

FString UAnimNotifyState_ActionLock::GetNotifyName_Implementation() const
{
	return TEXT("ActionLock");
}

void UAnimNotifyState_ActionLock::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (BlockedAbilityTags.IsEmpty() || !IsValid(MeshComp))
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(MeshComp->GetOwner()))
	{
		ASC->BlockAbilitiesWithTags(BlockedAbilityTags);
	}
}

void UAnimNotifyState_ActionLock::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (!BlockedAbilityTags.IsEmpty() && IsValid(MeshComp))
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(MeshComp->GetOwner()))
		{
			ASC->UnBlockAbilitiesWithTags(BlockedAbilityTags);
		}
	}

	Super::NotifyEnd(MeshComp, Animation, EventReference);
}
