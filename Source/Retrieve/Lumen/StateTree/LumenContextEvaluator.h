#pragma once

#include "CoreMinimal.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeExecutionTypes.h"
#include "LumenContextEvaluator.generated.h"

class AAIController;
class ULumenFollowComponent;

USTRUCT(BlueprintType)
struct FLumenContextEvalInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output")
	TObjectPtr<AActor> HostActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output")
	FVector HostLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output")
	float DistanceToHost = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output")
	bool bHostInCombat = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output")
	bool bWaitRequested = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output")
	bool bThreatNear = false;
	
	/** 위협 스캔 쓰로틀링용 누적 타이머 */
	UPROPERTY()
	float TimeSinceThreatScan = 0.f;

	UPROPERTY()
	bool bHostEngaged = false;


	UPROPERTY()
	TWeakObjectPtr<ULumenFollowComponent> CachedFollowComp = nullptr;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Lumen Context", Category = "Retrieve|AI"))
struct RETRIEVE_API FLumenContextEvaluator : public FStateTreeEvaluatorCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FLumenContextEvalInstanceData;

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual void TreeStart(FStateTreeExecutionContext& Context) const override;
	virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	// Legacy serialized setting. Retreat 판단은 이제 거리 대신 몬스터의 실제 인식 타깃을 사용합니다.
	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = "0.0"))
	float ThreatNearRadius = 700.f;

	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = "0.0"))
	float ThreatScanInterval = 0.2f;

	// Legacy serialized setting. Retreat 판단은 이제 거리 대신 몬스터의 실제 인식 타깃을 사용합니다.
	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = "0.0"))
	float HostCombatRadius = 3500.f;

private:
	TStateTreeExternalDataHandle<AAIController> AIControllerHandle;
	TStateTreeExternalDataHandle<APawn> PawnHandle;
};
