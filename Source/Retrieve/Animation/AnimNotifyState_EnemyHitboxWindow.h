#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_EnemyHitboxWindow.generated.h"

class UEnemyCombatComponent;

UCLASS(meta = (DisplayName = "Enemy Hitbox Window"))
class RETRIEVE_API UAnimNotifyState_EnemyHitboxWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitbox")
	FName BoneName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitbox")
	FVector Offset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitbox", meta = (ClampMin = "0.0"))
	float Radius = 30.f;
};
