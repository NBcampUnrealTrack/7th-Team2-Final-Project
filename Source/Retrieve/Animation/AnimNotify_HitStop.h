#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_HitStop.generated.h"

/**
 * 이 프레임에 카운터 대상(PendingCounterTarget)에게 짧은 히트스톱을 준다. 카운터 마무리 프레임에 배치.
 * 실제 처리는 CounterTimeDilationComponent(대상 시간만 낮춰 충돌 없음, Standalone 전용).
 */
UCLASS(DisplayName = "Hit Stop")
class RETRIEVE_API UAnimNotify_HitStop : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

	// 정지 실시간 지속(초). 짧게(0.05~0.1).
	UPROPERTY(EditAnywhere, Category = "Retrieve|Combat", meta = (ClampMin = "0.0"))
	float Duration = 0.06f;

	// 대상 시간 배율(0에 가까울수록 완전 정지).
	UPROPERTY(EditAnywhere, Category = "Retrieve|Combat", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float TimeScale = 0.05f;
};
