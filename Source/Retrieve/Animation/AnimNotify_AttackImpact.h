#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "AnimNotify_AttackImpact.generated.h"

/**
 * 
 */
UCLASS(DisplayName = "Attack Impact")
class RETRIEVE_API UAnimNotify_AttackImpact : public UAnimNotify
{
	GENERATED_BODY()
	
public:
	UAnimNotify_AttackImpact();
	
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Combat")
	FGameplayTag EventTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Combat", meta = (ClampMin = "0.0"))
	float ImpactMagnitude = 1.0f;
};
