
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LockOnCameraRig.generated.h"

class APawn;
class APlayerController;
class UCharacterMovementComponent;
class USpringArmComponent;
class ULockOnCameraConfig;

/**
 * 락온 카메라 추적 + 캐릭터 회전 모드 토글 책임 컴포넌트
 * UCombatReactionComponent가 BeginPlay에서 동적 부착(NewObject + RegisterComponent)
 * 파라미터는 ULockOnCameraConfig DataAsset으로 분리
 */
UCLASS(ClassGroup = "Retrieve", meta=(BlueprintSpawnableComponent))
class RETRIEVE_API ULockOnCameraRig : public UActorComponent
{
	GENERATED_BODY()

public:
	ULockOnCameraRig();
	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
							   FActorComponentTickFunction* ThisTickFunction) override;
	// ReactionComp가 BeginPlay에서 호출 OwnerPawn + 이동 컴포넌트 캐싱
	void Initialize();
	// 튜닝 파라미터 주입 (ReactionComp가 NewObject 직후 호출)
	void SetConfig(ULockOnCameraConfig* InConfig);
	// 락온 시작 모드 백업 후 스트레이프 모드로 전환 + Tick 활성
	void StartTracking(AActor* Target);
	// 락온 해제 백업 모드 복원 + Tick 비활성
	void StopTracking(bool bImmediateRestore = false);
	
	bool IsTracking() const { return bIsTracking; }
	
protected:
	// 튜닝 파라미터
	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|LockOnCamera")
	TObjectPtr<ULockOnCameraConfig> Config;

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
