#include "Animation/AnimNotify_HitStop.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Components/Player/CounterTimeDilationComponent.h"
#include "GameFramework/Actor.h"

FString UAnimNotify_HitStop::GetNotifyName_Implementation() const
{
	return TEXT("HitStop");
}

void UAnimNotify_HitStop::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* Owner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;
	if (!IsValid(Owner))
	{
		return;
	}

	URetrieveAbilitySystemComponent* ASC = Cast<URetrieveAbilitySystemComponent>(
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner));
	UCounterTimeDilationComponent* Comp = Owner->FindComponentByClass<UCounterTimeDilationComponent>();
	if (!IsValid(ASC) || !IsValid(Comp))
	{
		return;
	}

	if (AActor* Target = ASC->GetPendingCounterTarget())
	{
		Comp->DoHitStop(Target, Duration, TimeScale);
	}
}
