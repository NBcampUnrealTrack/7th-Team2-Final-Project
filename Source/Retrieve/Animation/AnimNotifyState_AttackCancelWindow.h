#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "AnimNotifyState_AttackCancelWindow.generated.h"

UCLASS(DisplayName = "Attack Cancel Window")
class RETRIEVE_API UAnimNotifyState_AttackCancelWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAnimNotifyState_AttackCancelWindow();

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Combat")
	FGameplayTag CancelOpenTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Combat")
	FGameplayTagContainer AllowedCancelIntents;
};
