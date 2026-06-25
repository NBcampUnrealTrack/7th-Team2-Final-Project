#include "Animation/AnimNotify_SetWeaponDrawn.h"

#include "Components/Player/WeaponComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_SetWeaponDrawn::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return;
	}

	if (UWeaponComponent* Weapon = Owner->FindComponentByClass<UWeaponComponent>())
	{
		Weapon->SetWeaponDrawn(bDrawn, TargetDrawnSocket);
	}
}

FString UAnimNotify_SetWeaponDrawn::GetNotifyName_Implementation() const
{
	return bDrawn ? TEXT("DrawWeapon") : TEXT("SheatheWeapon");
}
