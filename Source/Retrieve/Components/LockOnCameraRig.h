
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LockOnCameraRig.generated.h"

class APawn;
class APlayerController;
class UCharacterMovementComponent;
class USpringArmComponent;

/**
 * 락온 카메라 추적 + 캐릭터 회전 모드 토글 책임 컴포넌트
 * UCombatReactionComponent가 BeginPlay에서 동적 부착(NewObject + RegisterComponent)
 * 파라미터는 1주차 검증 후 ULockOnCameraConfig DataAsset으로 분리 예정
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RETRIEVE_API ULockOnCameraRig : public UActorComponent
{
	GENERATED_BODY()

public:
	ULockOnCameraRig();
	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
							   FActorComponentTickFunction* ThisTickFunction) override;
	// ReactionComp가 BeginPlay에서 호출 OwnerPawn + 이동 컴포넌트 캐싱
	void Initialize();
	// 락온 시작 모드 백업 후 스트레이프 모드로 전환 + Tick 활성
	void StartTracking(AActor* Target);
	// 락온 해제 백업 모드 복원 + Tick 비활성
	void StopTracking();
	
	bool IsTracking() const { return bIsTracking; }
	// 외부에 현재 타겟을 매 프레임 알려주는 함수 ReactionComp가 Tick에서 호출
	//void SetTarget(AActor* InTarget) { CurrentTarget = InTarget; }
	
protected:
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|LockOnCamera")
	float CameraInterpSpeed = 8.f;
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|LockOnCamera")
	float MinPitch = -30.f;
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|LockOnCamera")
	float MaxPitch = 10.f;
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|LockOnCamera")
	float TargetVerticalOffset = 60.f;
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|LockOnCamera|Offset")
	FVector LockOnSocketOffset = FVector(0.f, 60.f, 80.f);
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|LockOnCamera")
	float OffsetInterpSpeed = 6.f;
	
private:
	TWeakObjectPtr<APawn> OwnerPawn;
	TWeakObjectPtr<APlayerController> OwnerPC;
	TWeakObjectPtr<UCharacterMovementComponent> MovementComp;
	TWeakObjectPtr<AActor> CurrentTarget;
	TWeakObjectPtr<USpringArmComponent> SpringArm;
	FVector SavedSocketOffset = FVector::ZeroVector;
	
	bool bIsTracking = false;
	bool bIsReturning = false;
	bool bSavedUseControllerRotationYaw = false;
	bool bSavedOrientRotationToMovement = true;
	
};
