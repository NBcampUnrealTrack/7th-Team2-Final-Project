#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_EnemyMovementLock.generated.h"

UCLASS(DisplayName = "Enemy Movement Lock")
class RETRIEVE_API UAnimNotifyState_EnemyMovementLock : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};
