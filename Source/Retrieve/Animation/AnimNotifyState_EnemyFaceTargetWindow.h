#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_EnemyFaceTargetWindow.generated.h"

UCLASS(meta = (DisplayName = "Enemy Face Target Window"))
class RETRIEVE_API UAnimNotifyState_EnemyFaceTargetWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override;

	virtual void NotifyTick(USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float FrameDeltaTime,
		const FAnimNotifyEventReference& EventReference) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Face Target", meta = (ClampMin = "0.0"))
	float InterpSpeed = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Face Target")
	bool bYawOnly = true;
};
