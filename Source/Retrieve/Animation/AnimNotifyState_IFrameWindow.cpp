#include "Animation/AnimNotifyState_IFrameWindow.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotifyState_IFrameWindow::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	// 윈도우를 여는 Ability는 하나면 충분(중복 GE 방지).
	bool bOpened = false;
	ForEachActiveRetrieveAbility(MeshComp, [&bOpened](URetrieveGameplayAbility& Ability)
	{
		if (!bOpened)
		{
			bOpened = Ability.OpenNotifyIFrameWindow();
		}
	});
}

void UAnimNotifyState_IFrameWindow::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	// 활성 Ability에 Close 요청(각자 handle로 idempotent 종료).
	ForEachActiveRetrieveAbility(MeshComp, [](URetrieveGameplayAbility& Ability)
	{
		Ability.CloseNotifyIFrameWindow();
	});

	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UAnimNotifyState_IFrameWindow::GetNotifyName_Implementation() const
{
	return TEXT("IFrameWindow");
}

void UAnimNotifyState_IFrameWindow::ForEachActiveRetrieveAbility(
	const USkeletalMeshComponent* MeshComp,
	TFunctionRef<void(URetrieveGameplayAbility&)> Func) const
{
	if (!IsValid(MeshComp))
	{
		return;
	}

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!IsValid(OwnerActor))
	{
		return;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerActor);
	if (!IsValid(ASC))
	{
		return;
	}

	for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.IsActive())
		{
			continue;
		}

		URetrieveGameplayAbility* Ability = Cast<URetrieveGameplayAbility>(Spec.GetPrimaryInstance());
		if (IsValid(Ability) && Ability->IsActive())
		{
			Func(*Ability);
		}
	}
}
