#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_CameraZoom.generated.h"

/**
 * 카메라 붐 거리 구도를 카운터용으로 오버라이드한다(유저 줌 거리는 붐이 저장, bRestore 시 복귀).
 * 프레임마다 배치해 다단 연출(살짝→크게→원복) 가능.
 */
UCLASS(DisplayName = "Camera Zoom")
class RETRIEVE_API UAnimNotify_CameraZoom : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

	// true면 유저 저장 거리로 복귀(오버라이드 해제).
	UPROPERTY(EditAnywhere, Category = "Retrieve|Camera")
	bool bRestore = false;

	// 카메라 거리(절대값). 작을수록 확대.
	UPROPERTY(EditAnywhere, Category = "Retrieve|Camera", meta = (EditCondition = "!bRestore", ClampMin = "0.0"))
	float TargetArmLength = 200.f;

	// 보간 속도(클수록 빠름). bRestore=true면 복귀 속도.
	UPROPERTY(EditAnywhere, Category = "Retrieve|Camera", meta = (ClampMin = "0.1"))
	float ArmBlendSpeed = 12.f;
};
