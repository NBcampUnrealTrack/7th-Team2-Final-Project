#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_BurstHit.generated.h"

/**
 * 버스트 스킬의 히트 윈도우를 표현하는 NotifyState.
 *
 * 노티파이 구간 동안 NotifyTick 마다 Owner 의 PlayerBurstComponent::OnBurstHit(HitIndex) 를 호출한다.
 * HitIndex 는 FSkillCombination::HitSequence 의 인덱스에 대응한다.
 * 같은 몽타주에 HitIndex 가 다른 NotifyState 를 여러 개 박으면 멀티히트가 된다.
 */
UCLASS(DisplayName = "Burst Hit (State)")
class RETRIEVE_API UAnimNotifyState_BurstHit : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	/** FSkillCombination::HitSequence 의 인덱스. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Burst")
	int32 HitIndex = 0;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;
};
