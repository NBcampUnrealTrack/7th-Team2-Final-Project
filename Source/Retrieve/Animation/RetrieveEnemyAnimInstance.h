#pragma once

#include "Animation/AnimInstance.h"
#include "RetrieveEnemyAnimInstance.generated.h"

class APawn;

UCLASS(Blueprintable)
class RETRIEVE_API URetrieveEnemyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	UPROPERTY(BlueprintReadWrite, Category = "Enemy|Locomotion")
	float Speed = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "Enemy|Locomotion")
	float Direction = 0.f;

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> CachedPawnForMovementSample;

	FVector LastMovementSampleLocation = FVector::ZeroVector;
	bool bHasMovementSampleLocation = false;
};
