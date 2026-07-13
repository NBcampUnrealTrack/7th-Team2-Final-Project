#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_IFrameWindow.generated.h"

class URetrieveGameplayAbility;

/**
 * 몽타주 기반 무적(i-frame) 윈도우. ParryWindow와 동형의 중립 라우터 —
 * 활성 URetrieveGameplayAbility에 Open/CloseNotifyIFrameWindow hook만 호출(GA_Dash가 무적 GE를 여닫음).
 * 대시 몽타주의 무적 구간에 배치하면 그 구간만 전투 피해를 무시한다(환경/DoT 제외).
 */
UCLASS(DisplayName = "IFrame Window")
class RETRIEVE_API UAnimNotifyState_IFrameWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

private:
	void ForEachActiveRetrieveAbility(
		const USkeletalMeshComponent* MeshComp,
		TFunctionRef<void(URetrieveGameplayAbility&)> Func) const;
};
