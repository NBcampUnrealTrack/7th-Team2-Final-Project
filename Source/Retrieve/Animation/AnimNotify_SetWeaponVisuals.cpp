#include "Animation/AnimNotify_SetWeaponVisuals.h"

#include "Components/Player/WeaponComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

void UAnimNotify_SetWeaponVisuals::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner || Owner->GetLocalRole() == ROLE_SimulatedProxy)
	{
		return; // 원격은 OnRep 즉시 스폰 사용
	}

	if (UWeaponComponent* Weapon = Owner->FindComponentByClass<UWeaponComponent>())
	{
		if (bSpawn)
		{
			Weapon->SpawnWeaponVisuals();
		}
		else
		{
			Weapon->ClearWeaponVisuals();
		}
	}
}

FString UAnimNotify_SetWeaponVisuals::GetNotifyName_Implementation() const
{
	return bSpawn ? TEXT("SpawnWeapon") : TEXT("ClearWeapon");
}