#include "Animation/AnimNotifyState_BurstHit.h"

#include "Components/Player/PlayerBurstComponent.h"
#include "Logging/RetrieveLogChannels.h"

FString UAnimNotifyState_BurstHit::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("BurstHit[%d]"), HitIndex);
}

void UAnimNotifyState_BurstHit::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!IsValid(MeshComp))
	{
		return;
	}

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!IsValid(OwnerActor))
	{
		return;
	}

	UE_LOG(LogRetrieveCombat, Verbose,
		TEXT("[AnimNotifyState_BurstHit] Begin Owner=%s HitIndex=%d Duration=%.3f"),
		*GetNameSafe(OwnerActor), HitIndex, TotalDuration);
}

void UAnimNotifyState_BurstHit::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (!IsValid(MeshComp))
	{
		return;
	}

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!IsValid(OwnerActor))
	{
		return;
	}

	if (OwnerActor->GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	UPlayerBurstComponent* Combat = OwnerActor->FindComponentByClass<UPlayerBurstComponent>();
	if (!IsValid(Combat))
	{
		UE_LOG(LogRetrieveCombat, Warning,
			TEXT("[AnimNotifyState_BurstHit] PlayerBurstComponent not found on %s"),
			*GetNameSafe(OwnerActor));
		return;
	}

	Combat->OnBurstHit(HitIndex);
}
