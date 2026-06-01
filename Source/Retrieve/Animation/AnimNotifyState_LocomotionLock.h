#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_LocomotionLock.generated.h"

/**
 * 공격/피격/리액션 등의 몽타주 구간에 박아 쓰는 잠금 NotifyState.
 *
 * 회전 잠금: AAlsCharacter::SetLocomotionAction(LocomotionAction_Attack) 호출
 *   → ALS RefreshGroundedRotation/RefreshInAirRotation이 LocomotionAction.IsValid()면 회전 스킵
 *
 * 이동 잠금: ASC에 Animation_Lock_Movement LooseTag 부여
 *   → URetrieveCharacterMovementComponent::GetMaxSpeed가 0 반환
 *
 * 두 잠금은 체크박스로 독립 제어합니다.
 */
UCLASS(EditInlineNew, CollapseCategories, DisplayName = "Locomotion Lock", meta = (ShowWorldContextPin))
class RETRIEVE_API UAnimNotifyState_LocomotionLock : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAnimNotifyState_LocomotionLock();

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	                         float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	                       const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

protected:
	/** 회전 잠금: ALS LocomotionAction 태그 세팅 */
	UPROPERTY(EditAnywhere, Category = "Retrieve|Combat")
	bool bLockRotation = true;

	/** 이동 잠금: ASC에 Animation_Lock_Movement LooseTag 부여 */
	UPROPERTY(EditAnywhere, Category = "Retrieve|Combat")
	bool bLockMovement = false;
};
