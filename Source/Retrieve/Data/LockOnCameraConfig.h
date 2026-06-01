
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LockOnCameraConfig.generated.h"

/**
 * ULockOnCameraRig의 카메라 추적/오프셋 튜닝 파라미터
 */
UCLASS(BlueprintType)
class RETRIEVE_API ULockOnCameraConfig : public UDataAsset
{
	GENERATED_BODY()
	
public:
	// Tracking
	// 컨트롤러 회전을 타겟 방향으로 보간하는 속도 (RInterpTo)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tracking")
	float CameraInterpSpeed = 8.f;
	// 락온 시 카메라 피치 하한 (아래로 내려가는 한계)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tracking")
	float MinPitch = -30.f;
	// 락온 시 카메라 피치 상한 (위로 올라가는 한계)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tracking")
	float MaxPitch = 10.f;
	// 타겟 위치 기준 바라볼 높이 오프셋 (가슴/머리 높이 조준용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tracking")
	float TargetVerticalOffset = 60.f;
	// Offset
	// 락온 중 SpringArm SocketOffset 목표값
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Offset")
	FVector LockOnSocketOffset = FVector(0.f, 60.f, 80.f);
	// SocketOffset 보간 속도 (진입/복원 공용, VInterpTo)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Offset")
	float OffsetInterpSpeed = 6.f;
};
