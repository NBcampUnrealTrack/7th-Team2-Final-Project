#include "Animation/AnimNotify_SetNockedArrow.h"

#include "Components/Player/WeaponComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_SetNockedArrow::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
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
		Weapon->SetNockedArrowVisible(bVisible);
	}
}

FString UAnimNotify_SetNockedArrow::GetNotifyName_Implementation() const
{
	return bVisible ? TEXT("ShowNockedArrow") : TEXT("HideNockedArrow");
}