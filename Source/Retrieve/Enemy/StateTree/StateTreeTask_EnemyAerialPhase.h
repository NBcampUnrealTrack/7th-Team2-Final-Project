#pragma once

#include "CoreMinimal.h"
#include "StateTreeExecutionTypes.h"
#include "StateTreeTaskBase.h"
#include "StateTreeTask_EnemyAerialPhase.generated.h"

class APawn;
class ARetrieveEnemyCharacter;
class UAnimSequenceBase;

USTRUCT(BlueprintType)
struct FStateTreeTask_EnemyAerialPhaseInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta=(Optional))
	TObjectPtr<AActor> TargetPlayer = nullptr;

	UPROPERTY(EditAnywhere, Category="Config", meta=(ClampMin="200.0"))
	float HoverHeight = 700.f;

	UPROPERTY(EditAnywhere, Category="Config", meta=(ClampMin="10.0"))
	float PositionTolerance = 120.f;

	UPROPERTY(EditAnywhere, Category="Config", meta=(ClampMin="0.0"))
	float AttackDelay = 1.25f;

	UPROPERTY(EditAnywhere, Category="Config", meta=(ClampMin="0.5"))
	float MaxPhaseDuration = 10.f;

	UPROPERTY(EditAnywhere, Category="Config", meta=(ClampMin="0.0"))
	float ReentryLockDuration = 8.f;

	UPROPERTY(EditAnywhere, Category="Animation")
	TSoftObjectPtr<UAnimSequenceBase> TakeOffAnimation = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/External/PolygonalCreaturesPack/PolygonalDragon/Animations/Polygonal_Dragon_AnimationFly_Up.Polygonal_Dragon_AnimationFly_Up")));

	UPROPERTY(EditAnywhere, Category="Animation")
	TSoftObjectPtr<UAnimSequenceBase> FlightAnimation = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/External/PolygonalCreaturesPack/PolygonalDragon/Animations/Polygonal_Dragon_AnimationFly_Forward.Polygonal_Dragon_AnimationFly_Forward")));

	UPROPERTY(EditAnywhere, Category="Animation")
	TSoftObjectPtr<UAnimSequenceBase> HoverAnimation = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/External/PolygonalCreaturesPack/PolygonalDragon/Animations/Polygonal_Dragon_AnimationFly_Idle.Polygonal_Dragon_AnimationFly_Idle")));

	UPROPERTY(EditAnywhere, Category="Animation", meta=(ClampMin="0.01"))
	float MontagePlayRate = 1.f;

	UPROPERTY(EditAnywhere, Category="Input", meta=(Optional))
	bool bHasAerialPhase = false;

	float GroundZ = 0.f;
	float ElapsedTime = 0.f;
	FVector HoverLocation = FVector::ZeroVector;
	bool bPlayedFlightMontage = false;
	bool bPlayedHoverMontage = false;

	UPROPERTY()
	TWeakObjectPtr<ARetrieveEnemyCharacter> CachedEnemy = nullptr;
};

USTRUCT(BlueprintType, meta=(DisplayName="Enemy Aerial Phase", Category="Retrieve|AI"))
struct RETRIEVE_API FStateTreeTask_EnemyAerialPhase : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeTask_EnemyAerialPhaseInstanceData;

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

private:
	TStateTreeExternalDataHandle<APawn> PawnHandle;
};
