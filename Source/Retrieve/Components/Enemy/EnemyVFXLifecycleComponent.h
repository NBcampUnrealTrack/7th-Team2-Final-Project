#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemyVFXLifecycleComponent.generated.h"

class UNiagaraComponent;

UCLASS(ClassGroup = "Retrieve")
class RETRIEVE_API UEnemyVFXLifecycleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyVFXLifecycleComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void RegisterVFX(UNiagaraComponent* NiagaraComponent);
	void UnregisterVFX(UNiagaraComponent* NiagaraComponent);
	void CleanupAllVFX();

private:
	void PruneInvalidVFX();

	TSet<TWeakObjectPtr<UNiagaraComponent>> RegisteredVFX;
};
