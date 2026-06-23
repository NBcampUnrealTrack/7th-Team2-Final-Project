#include "Animation/AnimNotifyState_AttackCancelWindow.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"

UAnimNotifyState_AttackCancelWindow::UAnimNotifyState_AttackCancelWindow()
{
	CancelOpenTag = RetrieveGameplayTags::State_Attack_CancelOpen;
}

FString UAnimNotifyState_AttackCancelWindow::GetNotifyName_Implementation() const
{
	return TEXT("AttackCancelWindow");
}

void UAnimNotifyState_AttackCancelWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!IsValid(MeshComp) || !CancelOpenTag.IsValid())
	{
		return;
	}

	if (AActor* OwnerActor = MeshComp->GetOwner())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwnerActor))
		{
			if (URetrieveAbilitySystemComponent* RetrieveASC = Cast<URetrieveAbilitySystemComponent>(ASC))
			{
				RetrieveASC->AddAttackCancelWindow(CancelOpenTag, AllowedCancelIntents);
			}
			else
			{
				ASC->AddLooseGameplayTag(CancelOpenTag);
			}
		}
	}
}

void UAnimNotifyState_AttackCancelWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!IsValid(MeshComp) || !CancelOpenTag.IsValid())
	{
		return;
	}

	if (AActor* OwnerActor = MeshComp->GetOwner())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwnerActor))
		{
			if (URetrieveAbilitySystemComponent* RetrieveASC = Cast<URetrieveAbilitySystemComponent>(ASC))
			{
				RetrieveASC->RemoveAttackCancelWindow(CancelOpenTag, AllowedCancelIntents);
			}
			else
			{
				ASC->RemoveLooseGameplayTag(CancelOpenTag);
			}
		}
	}
}
