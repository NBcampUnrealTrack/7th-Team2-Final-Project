#include "Animation/AnimNotifyState_ComboWindow.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"

UAnimNotifyState_ComboWindow::UAnimNotifyState_ComboWindow()
{
	ComboOpenTag = RetrieveGameplayTags::State_Combo_Open;
}

FString UAnimNotifyState_ComboWindow::GetNotifyName_Implementation() const
{
	return TEXT("ComboWindow");
}

void UAnimNotifyState_ComboWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!IsValid(MeshComp) || !ComboOpenTag.IsValid())
	{
		return;
	}
	
	if (AActor* OwnerActor = MeshComp->GetOwner())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwnerActor))
		{
			ASC->AddLooseGameplayTag(ComboOpenTag);
		}
	}
}

void UAnimNotifyState_ComboWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!IsValid(MeshComp) || !ComboOpenTag.IsValid())
	{
		return;
	}
	
	if (AActor* OwnerActor = MeshComp->GetOwner())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwnerActor))
		{
			ASC->RemoveLooseGameplayTag(ComboOpenTag);
		}
	}
}
