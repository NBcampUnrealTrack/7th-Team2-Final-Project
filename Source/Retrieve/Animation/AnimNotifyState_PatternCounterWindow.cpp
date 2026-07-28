#include "Animation/AnimNotifyState_PatternCounterWindow.h"

#include "Components/Enemy/PatternCounterComponent.h"

FString UAnimNotifyState_PatternCounterWindow::GetNotifyName_Implementation() const
{
	return TEXT("PatternCounterWindow");
}

void UAnimNotifyState_PatternCounterWindow::NotifyBegin(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	AActor* OwnerActor = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (!IsValid(OwnerActor))
	{
		return;
	}

	if (UPatternCounterComponent* PatternCounter = OwnerActor->FindComponentByClass<UPatternCounterComponent>())
	{
		PatternCounter->OpenCounterWindow(TotalDuration);
	}
}

void UAnimNotifyState_PatternCounterWindow::NotifyEnd(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	AActor* OwnerActor = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (!IsValid(OwnerActor))
	{
		return;
	}

	if (UPatternCounterComponent* PatternCounter = OwnerActor->FindComponentByClass<UPatternCounterComponent>())
	{
		PatternCounter->CloseCounterWindow();
	}
}
