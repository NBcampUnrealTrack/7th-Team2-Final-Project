#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Combat/RetrieveCombatTypes.h"
#include "AnimNotifyState_EnemyRadialKnockback.generated.h"

UCLASS(meta = (DisplayName = "Enemy Radial Knockback Window"))
class RETRIEVE_API UAnimNotifyState_EnemyRadialKnockback : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override;

	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyTick(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float FrameDeltaTime,
		const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knockback|Shape")
	FName BoneName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knockback|Shape")
	FVector Offset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knockback|Shape", meta = (ClampMin = "0.0"))
	float Radius = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knockback|Timing")
	bool bApplyOnBegin = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knockback|Timing", meta = (ClampMin = "0.0"))
	float ApplyInterval = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knockback|Params")
	FRetrieveKnockbackParams KnockbackParams;

private:
	FVector ResolveCenter(USkeletalMeshComponent* MeshComp) const;
	void ApplyKnockback(USkeletalMeshComponent* MeshComp) const;

	UPROPERTY(Transient)
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, float> ElapsedTimeByMesh;
};
