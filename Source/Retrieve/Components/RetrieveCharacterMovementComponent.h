#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"

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
class RETRIEVE_API URetrieveCharacterMovementComponent : public UCharacterMovementComponent
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

protected:
	virtual void InitializeComponent() override;

	// GetGroundInfo()를 통해 필요할 때만 갱신되는 지면 정보 캐시입니다.
	FRetrieveCharacterGroundInfo CachedGroundInfo;

	UPROPERTY(Transient)
	bool bHasReplicatedAcceleration = false;
};
