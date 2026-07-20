#include "Animation/AnimNotify_CameraZoom.h"

#include "Components/Pawn/RetrieveCameraBoom.h"
#include "GameFramework/Actor.h"

FString UAnimNotify_CameraZoom::GetNotifyName_Implementation() const
{
	return TEXT("CameraZoom");
}

void UAnimNotify_CameraZoom::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* Owner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;
	URetrieveCameraBoom* Boom = Owner ? Owner->FindComponentByClass<URetrieveCameraBoom>() : nullptr;
	if (!IsValid(Boom))
	{
		return;
	}

	// GA_ParryCounter::EndAbility의 해제와 같은 ID를 써야 한다.
	static const FName CounterOverrideId(TEXT("ParryCounter"));

	if (bRestore)
	{
		Boom->ClearCameraBoomProfileOverride(CounterOverrideId, ArmBlendSpeed); // 복귀 속도 지정.
		return;
	}

	FRetrieveCameraBoomProfile Profile;
	Profile.TargetArmLength = TargetArmLength;
	Profile.ArmBlendSpeed = ArmBlendSpeed;
	Boom->SetCameraBoomProfileOverride(CounterOverrideId, Profile);
}
