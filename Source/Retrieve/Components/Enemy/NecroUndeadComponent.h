
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NecroUndeadComponent.generated.h"


UCLASS(ClassGroup = "Retrieve", meta=(BlueprintSpawnableComponent))
class RETRIEVE_API UNecroUndeadComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNecroUndeadComponent();
	// 소환 GA가 스폰 직후 호출
	void RegisterUndead(APawn* Undead);
	
	UFUNCTION(BlueprintPure, Category = "Necro|Undead")
	int32 GetLiveUndeadCount() const;

protected:
	virtual void BeginPlay() override;
	virtual  void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleOwnerDeathStarted(AActor* OwningActor);

	void CompactUndeads();
	void DetonateAllUndaeds();

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<APawn>> Undeads;

	bool bDeathHandled = false;
};
