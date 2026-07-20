#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_CameraShake.generated.h"

class UCameraShakeBase;

/**
 * 이 프레임에 로컬 시점 카메라 셰이크를 재생한다.
 * 연출 전용 — 접근성/유저 셰이크 스케일을 무시하고 항상 지정 강도로 재생한다.
 */
UCLASS(DisplayName = "Camera Shake")
class RETRIEVE_API UAnimNotify_CameraShake : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

	UPROPERTY(EditAnywhere, Category = "Retrieve|Camera")
	TSubclassOf<UCameraShakeBase> CameraShake;

	UPROPERTY(EditAnywhere, Category = "Retrieve|Camera", meta = (ClampMin = "0.0"))
	float Scale = 1.f;
};
