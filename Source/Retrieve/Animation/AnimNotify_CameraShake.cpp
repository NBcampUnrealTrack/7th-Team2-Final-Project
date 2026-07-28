#include "Animation/AnimNotify_CameraShake.h"

#include "Camera/CameraShakeBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

FString UAnimNotify_CameraShake::GetNotifyName_Implementation() const
{
	return TEXT("CameraShake");
}

void UAnimNotify_CameraShake::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!CameraShake || !IsValid(MeshComp))
	{
		return;
	}

	const APawn* Pawn = Cast<APawn>(MeshComp->GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (IsValid(PC) && PC->IsLocalController())
	{
		PC->ClientStartCameraShake(CameraShake, Scale);
	}
}
