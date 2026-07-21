
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SpringArmComponent.h"
#include "RetrieveCameraBoom.generated.h"


USTRUCT(BlueprintType)
struct RETRIEVE_API FRetrieveCameraBoomProfile
{
	GENERATED_BODY()

	// 스프링암 목표 길이. 실제 카메라 거리는 충돌 보정 후 짧아질 수 있으므로,
	// 이 값은 "원래 의도한 카메라 거리"로만 다룬다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Camera")
	float TargetArmLength = 300.f;

	// 카메라 컴포넌트의 RelativeLocation 오프셋.
	// SpringArm의 SocketOffset을 바꾸면 프로브/충돌 계산에도 영향을 줄 수 있으므로,
	// 전투 시점 보정은 자식 카메라 위치에서 처리한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Camera")
	FVector CameraRelativeOffset = FVector::ZeroVector;

	// TargetArmLength로 보간하는 속도. 클수록 빠르게 목표 줌에 도달한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Camera", meta=(ClampMin="0.1"))
	float ArmBlendSpeed = 10.f;

	// CameraRelativeOffset으로 보간하는 속도. 클수록 빠르게 목표 숄더 위치에 도달한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Camera", meta=(ClampMin="0.1"))
	float OffsetBlendSpeed = 8.f;
};


UCLASS(ClassGroup = "Retrieve", meta=(BlueprintSpawnableComponent))
class RETRIEVE_API URetrieveCameraBoom : public USpringArmComponent
{
	GENERATED_BODY()

public:
	void AddZoomInput(float AxisValue);

	// 어빌리티/연출 등 외부 시스템이 일시적으로 카메라 구도를 바꾸는 진입점.
	// 동일 ID로 다시 호출하면 현재 override를 갱신하고, 다른 ID로 호출하면 새 override가 우선한다.
	void SetCameraBoomProfileOverride(FName OverrideId, const FRetrieveCameraBoomProfile& Profile);

	// SetCameraBoomProfileOverride로 등록한 구도를 해제한다.
	// 현재 활성 override와 ID가 다르면 무시해서, 늦게 끝난 어빌리티가 다른 구도를 지우지 않게 한다.
	// RestoreArmBlendSpeed >= 0이면 원래 거리로 돌아가는 보간 속도를 이 값으로 덮어쓴다(작을수록 천천히 복귀).
	void ClearCameraBoomProfileOverride(FName OverrideId, float RestoreArmBlendSpeed = -1.f);

protected:
	// 스프링암이 충돌/지연 계산을 끝낸 직후 호출됨
	virtual void UpdateDesiredArmLocation(
		bool bDoTrace, bool bDoLocationLag, bool bDoRotationLag, float DeltaTime) override;
	// 평소(충돌 없음) 오프셋 — 0이면 센터뷰
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Camera")
	FVector BaseOffset = FVector::ZeroVector;
	// 벽에 막혀 카메라가 당겨졌을 때 적용할 숄더 오프셋 (Y=옆, Z=위)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Camera")
	FVector ShoulderOffset = FVector(0.f, 50.f, 30.f);
	// 전환 부드러움 (클수록 빠르게)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Camera", meta=(ClampMin="0.1"))
	float ShoulderBlendSpeed = 8.f;

	// 줌 인/아웃 관련 변수
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Camera|Zoom")
	float MinArmLength = 150.f;
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Camera|Zoom")
	float MaxArmLength = 600.f;
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Camera|Zoom")
	float ZoomStep = 50.f;
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Camera|Zoom")
	float ZoomInterpSpeed = 10.f;

	// 카메라가 강 수면 아래로 내려갈 때 올려둘 최저 높이 = 물 메시 수면 + 이 여유값
	// (입수 스플래시·물결로 시각 수면이 콜리전보다 솟으므로 그 위로 띄울 만큼 줘야 안 보임)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Camera|Water")
	float WaterSurfaceCameraMargin = 40.f;
	// 물 메시 표면을 읽을 트레이스 채널 (강 물 메시만 Block). 기본 WaterCamera.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Camera|Water")
	TEnumAsByte<ECollisionChannel> WaterTraceChannel = ECC_GameTraceChannel2;
	// 카메라 위에서 시작 / 아래로 끝나는 수직 트레이스 범위
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Camera|Water", meta=(ClampMin="0"))
	float WaterTraceUp = 2000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Camera|Water", meta=(ClampMin="0"))
	float WaterTraceDown = 2000.f;
	// 트레이스 미스가 이 시간 내면 스플라인 콜리전 빈틈으로 보고 직전 수면Z 유지(통과 방지 안전망)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Camera|Water", meta=(ClampMin="0"))
	float WaterMissHoldTime = 0.2f;

private:
	bool IsOwnerLockedOn() const;
	void CacheReturnCameraProfileIfNeeded();
	float GetCurrentDesiredArmLength() const;
	FVector GetDefaultCameraRelativeOffset() const;
	float GetResolvedArmTargetLength() const;
	float GetResolvedArmBlendSpeed() const;
	FVector GetResolvedCameraRelativeOffset() const;
	float GetResolvedCameraOffsetBlendSpeed() const;
	void ClearReturnCameraProfileIfRestored();

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> ChildCamera;

	float DesiredArmLength = -1.f;

	// 외부 override를 해제했을 때 돌아갈 기준 구도.
	// BowAim처럼 일시적인 구도는 플레이어 줌 상태를 덮어쓰면 안 되므로,
	// override 진입 직전의 목표 거리/오프셋을 저장해 두고 해제 시 그 값으로 보간한다.
	bool bHasReturnCameraProfile = false;
	FRetrieveCameraBoomProfile ReturnCameraProfile;

	// 현재 활성화된 외부 카메라 구도.
	// 지금은 단일 슬롯만 둔다. 락온은 이 슬롯보다 우선순위가 높고,
	// 여러 어빌리티가 동시에 구도를 잡아야 할 때만 스택/우선순위 구조로 확장한다.
	bool bHasCameraProfileOverride = false;
	FName ActiveCameraProfileOverrideId = NAME_None;
	FRetrieveCameraBoomProfile ActiveCameraProfileOverride;

	// 트레이스 미스 시 빈틈 메우기용 직전 수면Z / 미스 지속 시간
	float LastWaterSurfaceZ = 0.f;
	float WaterMissTime = 1.e9f;
};
