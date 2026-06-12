#pragma once

#include "CoreMinimal.h"
#include "AlsCharacterMovementComponent.h"

#include "RetrieveCharacterMovementComponent.generated.h"

USTRUCT(BlueprintType)
struct FRetrieveCharacterGroundInfo
{
	GENERATED_BODY()

	FRetrieveCharacterGroundInfo()
		: LastUpdateFrame(0)
		, GroundDistance(0.0f)
	{
	}

	uint64 LastUpdateFrame;

	UPROPERTY(BlueprintReadOnly)
	FHitResult GroundHitResult;

	UPROPERTY(BlueprintReadOnly)
	float GroundDistance;
};

UCLASS(Config = Game)
class RETRIEVE_API URetrieveCharacterMovementComponent : public UAlsCharacterMovementComponent
{
	GENERATED_BODY()

public:
	URetrieveCharacterMovementComponent(const FObjectInitializer& ObjectInitializer);

	virtual void SimulateMovement(float DeltaTime) override;
	virtual bool CanAttemptJump() const override;

	UFUNCTION(BlueprintCallable, Category = "Retrieve|CharacterMovement")
	const FRetrieveCharacterGroundInfo& GetGroundInfo();

	void SetReplicatedAcceleration(const FVector& InAcceleration);

	virtual FRotator GetDeltaRotation(float DeltaTime) const override;
	virtual float GetMaxSpeed() const override;
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;

	/** SwimDetection이 매 틱 push하는 수면 Z. 부력 깊이 오차에 사용. */
	void SetWaterSurfaceZ(float InSurfaceZ) { WaterSurfaceZ = InSurfaceZ; }
	void NotifySwimEntry();  // 입수 전환 시 SwimDetection이 호출 - 착수 속도로 플런지 판정
	bool IsPlunging() const { return bPlunging; }

protected:
	virtual void InitializeComponent() override;

	// GetGroundInfo()를 통해 필요할 때만 갱신되는 지면 정보 캐시입니다.
	FRetrieveCharacterGroundInfo CachedGroundInfo;

	UPROPERTY(Transient)
	bool bHasReplicatedAcceleration = false;

	// 수영 튜너블은 전부 URetrieveSwimSettings(Project Settings > Retrieve > Swim)로 이전됨.

	UPROPERTY(Transient)
	bool bPlunging = false;

	UPROPERTY(Transient)
	float WaterSurfaceZ = 0.f;

	UPROPERTY(Transient)
	float PrevWaterSurfaceZ = 0.f; // 수면 추종 피드포워드용 직전 프레임 수면 Z

	UPROPERTY(Transient)
	float PrevSurfaceVelZ = 0.f; // 직전 프레임 피드포워드 속도(누적 방지용 제거값)
};
