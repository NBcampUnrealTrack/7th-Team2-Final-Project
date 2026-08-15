#include "Components/Enemy/EnemyVFXLifecycleComponent.h"

#include "NiagaraComponent.h"

UEnemyVFXLifecycleComponent::UEnemyVFXLifecycleComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnemyVFXLifecycleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CleanupAllVFX();
	Super::EndPlay(EndPlayReason);
}

void UEnemyVFXLifecycleComponent::RegisterVFX(UNiagaraComponent* NiagaraComponent)
{
	if (!IsValid(NiagaraComponent))
	{
		return;
	}

	PruneInvalidVFX();
	RegisteredVFX.Add(NiagaraComponent);
}

void UEnemyVFXLifecycleComponent::UnregisterVFX(UNiagaraComponent* NiagaraComponent)
{
	if (NiagaraComponent)
	{
		RegisteredVFX.Remove(NiagaraComponent);
	}
	PruneInvalidVFX();
}

void UEnemyVFXLifecycleComponent::CleanupAllVFX()
{
	for (const TWeakObjectPtr<UNiagaraComponent>& WeakComponent : RegisteredVFX)
	{
		if (UNiagaraComponent* NiagaraComponent = WeakComponent.Get(); IsValid(NiagaraComponent))
		{
			NiagaraComponent->DeactivateImmediate();
			NiagaraComponent->DestroyComponent();
		}
	}

	RegisteredVFX.Empty();
}

void UEnemyVFXLifecycleComponent::PruneInvalidVFX()
{
	for (auto It = RegisteredVFX.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}
}
