
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SpringArmComponent.h"
#include "RetrieveCameraBoom.generated.h"


UCLASS(ClassGroup = "Retrieve", meta=(BlueprintSpawnableComponent))
class RETRIEVE_API URetrieveCameraBoom : public USpringArmComponent
{
	GENERATED_BODY()

public:
	void AddZoomInput(float AxisValue);

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

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> ChildCamera;

	float DesiredArmLength = -1.f;

	// 트레이스 미스 시 빈틈 메우기용 직전 수면Z / 미스 지속 시간
	float LastWaterSurfaceZ = 0.f;
	float WaterMissTime = 1.e9f;
};
