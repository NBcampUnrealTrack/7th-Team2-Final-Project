#include "Animation/AnimNotifyState_LocomotionLock.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AlsCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"

UAnimNotifyState_LocomotionLock::UAnimNotifyState_LocomotionLock()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(220, 60, 60);
#endif
}

void UAnimNotifyState_LocomotionLock::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                                   float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp)
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return;
	}

	if (bLockRotation)
	{
		if (AAlsCharacter* Als = Cast<AAlsCharacter>(Owner))
		{
			Als->SetLocomotionAction(RetrieveGameplayTags::LocomotionAction_Attack);
		}
	}

	if (bLockMovement)
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner))
		{
			ASC->AddLooseGameplayTag(RetrieveGameplayTags::Animation_Lock_Movement);
		}
	}
}

void UAnimNotifyState_LocomotionLock::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                                 const FAnimNotifyEventReference& EventReference)
{
	if (MeshComp)
	{
		if (AActor* Owner = MeshComp->GetOwner())
		{
			if (bLockRotation)
			{
				if (AAlsCharacter* Als = Cast<AAlsCharacter>(Owner))
				{
					// Empty tag = 잠금 해제 (ALS RefreshGroundedRotation의 IsValid() 체크가 false가 됨)
					Als->SetLocomotionAction(FGameplayTag::EmptyTag);
				}
			}

			if (bLockMovement)
			{
				if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner))
				{
					ASC->RemoveLooseGameplayTag(RetrieveGameplayTags::Animation_Lock_Movement);
				}
			}
		}
	}

	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UAnimNotifyState_LocomotionLock::GetNotifyName_Implementation() const
{
	// 에디터 트랙에서 "Lock(R,M)" 식으로 어떤 잠금이 켜졌는지 한눈에 보이게 표시
	FString Tags;
	if (bLockRotation) Tags += TEXT("R");
	if (bLockMovement) Tags += TEXT("M");
	return Tags.IsEmpty() ? TEXT("Lock(none)") : FString::Printf(TEXT("Lock(%s)"), *Tags);
}
