#include "AbilitySystem/Cues/GCN_HitFlash.h"

#include "Components/MeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"

bool UGCN_HitFlash::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	UWorld* World = MyTarget ? MyTarget->GetWorld() : nullptr;
	UMaterialInterface* Flash = FlashMaterial.LoadSynchronous();
	if (!World || !Flash)
	{
		return false;
	}
	
	struct FFlashedMesh
	{
		TWeakObjectPtr<UMeshComponent> Mesh;
		TWeakObjectPtr<UMaterialInterface> PreviousOverlay;
	};
	TArray<FFlashedMesh> Flashed;

	TArray<UMeshComponent*> MeshComponents;
	MyTarget->GetComponents<UMeshComponent>(MeshComponents);
	for (UMeshComponent* Mesh : MeshComponents)
	{
		if (!Mesh || Mesh->GetOverlayMaterial() == Flash)
		{
			continue;
		}
		Flashed.Add({Mesh, Mesh->GetOverlayMaterial()});
		Mesh->SetOverlayMaterial(Flash);
	}

	if (Flashed.IsEmpty())
	{
		return false;
	}
	
	FTimerHandle UnusedHandle;
	World->GetTimerManager().SetTimer(UnusedHandle,
		FTimerDelegate::CreateLambda([Flashed = MoveTemp(Flashed), FlashWeak = TWeakObjectPtr<UMaterialInterface>(Flash)]()
		{
			for (const FFlashedMesh& Entry : Flashed)
			{
				UMeshComponent* Mesh = Entry.Mesh.Get();
				
				if (Mesh && Mesh->GetOverlayMaterial() == FlashWeak.Get())
				{
					Mesh->SetOverlayMaterial(Entry.PreviousOverlay.Get());
				}
			}
		}),
		FlashDuration, false);

	return true;
}
