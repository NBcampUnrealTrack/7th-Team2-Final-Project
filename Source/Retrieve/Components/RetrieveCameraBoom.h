
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
	
private:
	bool IsOwnerLockedOn() const;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> ChildCamera;

	float DesiredArmLength = -1.f;
};
