#pragma once

#include "CoreMinimal.h"
#include "AlsLinkedAnimationInstance.h"

#include "RetrieveSwimmingLinkedAnimInstance.generated.h"

class URetrieveAlsAnimInstance;

/** Swimming-only linked instance, separated from weapon montage ownership. */
UCLASS()
class RETRIEVE_API URetrieveSwimmingLinkedAnimInstance : public UAlsLinkedAnimationInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;

protected:
	UFUNCTION(BlueprintPure, Category = "Retrieve|Linked Anim",
		Meta = (BlueprintThreadSafe, ReturnDisplayName = "Retrieve Parent"))
	URetrieveAlsAnimInstance* GetRetrieveParent() const;

	UPROPERTY(VisibleAnywhere, Category = "State", Transient)
	TWeakObjectPtr<URetrieveAlsAnimInstance> RetrieveParent;
};
