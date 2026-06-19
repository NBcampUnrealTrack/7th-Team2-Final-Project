#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "UObject/Class.h"
#include "StateTreeTask_LumenFollowHost.generated.h"

class ULumenFollowComponent;

USTRUCT(BlueprintType)
struct FStateTreeTask_LumenFollowHostInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input", meta = (Optional))
	TObjectPtr<AActor> HostActor = nullptr;

	UPROPERTY(EditAnywhere, Category = "Input")
	float DistanceToHost = 0.f;
	UPROPERTY(EditAnywhere, Category = "Follow", meta = (ClampMin = "0.0"))
	float FollowBandWidth = 150.f;
	UPROPERTY(EditAnywhere, Category = "Follow|Speed", meta = (ClampMin = "0.0"))
	float WalkSpeed = 250.f;
	UPROPERTY(EditAnywhere, Category = "Follow|Speed", meta = (ClampMin = "0.0"))
	float JogSpeed = 600.f;
	UPROPERTY(EditAnywhere, Category = "Follow|Speed", meta = (ClampMin = "0.0"))
	float SprintSpeed = 800.f;
	UPROPERTY(EditAnywhere, Category = "Follow|Speed", meta = (ClampMin = "0.0"))
	float JogBandDistance = 600.f;
	UPROPERTY(EditAnywhere, Category = "Follow|Speed", meta = (ClampMin = "0.0"))
	float SprintBandDistance = 1200.f;
	UPROPERTY(EditAnywhere, Category = "Follow|Speed", meta = (ClampMin = "0.1"))
	float SpeedInterpRate = 6.f;
	UPROPERTY(EditAnywhere, Category = "Follow", meta = (ClampMin = "0.0"))
	float StuckRecoverSeconds = 2.0f;

	float StuckTime = 0.f;

	UPROPERTY()
	TWeakObjectPtr<ULumenFollowComponent> CachedComp = nullptr;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Lumen Follow Host", Category = "Retrieve|AI"))
struct RETRIEVE_API FStateTreeTask_LumenFollowHost : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeTask_LumenFollowHostInstanceData;

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
	                                       const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

private:
	TStateTreeExternalDataHandle<APawn> PawnHandle;
};
 