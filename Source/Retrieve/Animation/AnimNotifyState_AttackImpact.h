#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "AnimNotifyState_AttackImpact.generated.h"

/**
 * 
 */
UCLASS(DisplayName = "Attack Impact (State)")
class RETRIEVE_API UAnimNotifyState_AttackImpact : public UAnimNotifyState
{
	GENERATED_BODY()
	
public:
	UAnimNotifyState_AttackImpact();

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Combat")
	FGameplayTag EventTag;
};
