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

	/** SwimDetection이 매 틱 push: 수면Z + 잠수비율(0~1) + 물기둥 진입 + 완전잠수. 부력·항력·점프게이트에 사용. */
	void SetWaterState(float InSurfaceZ, float InSubmersion, bool bInColumn, bool bFullySubmerged);
	/** 물 영역 이탈 시 호출 - 항력/점프게이트 상태 리셋. */
	void ClearWaterState();
	void NotifySwimEntry();  // 입수 전환 시 SwimDetection이 호출 - 착수 속도로 플런지 판정
	bool IsPlunging() const { return bPlunging; }
	/** SwimDetection이 캐릭터 위치에서 샘플해 push한 수면 월드 Z (안정적). 수영 중일 때 유효. */
	float GetWaterSurfaceZ() const { return WaterSurfaceZ; }

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
	float WaterSubmersion = 0.f; // 0=발끝 잠김 ~ 1=완전 잠수. Wade 항력 배율에 사용

	UPROPERTY(Transient)
	bool bInWaterColumn = false; // 물기둥 안(보행 항력 적용 조건)

	UPROPERTY(Transient)
	bool bWaterFullySubmerged = false; // 캡슐 상단 < 수면 = 점프 차단

	UPROPERTY(Transient)
	float PrevWaterSurfaceZ = 0.f; // 수면 추종 피드포워드용 직전 프레임 수면 Z

	UPROPERTY(Transient)
	float PrevSurfaceVelZ = 0.f; // 직전 프레임 피드포워드 속도(누적 방지용 제거값)
};
