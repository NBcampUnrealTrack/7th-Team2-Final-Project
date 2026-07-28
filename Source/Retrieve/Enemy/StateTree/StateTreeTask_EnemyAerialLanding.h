#pragma once

#include "CoreMinimal.h"
#include "StateTreeExecutionTypes.h"
#include "StateTreeTaskBase.h"
#include "StateTreeTask_EnemyAerialLanding.generated.h"

class APawn;
class ARetrieveEnemyCharacter;
class UAnimSequenceBase;

USTRUCT(BlueprintType)
struct FStateTreeTask_EnemyAerialLandingInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Animation")
	TSoftObjectPtr<UAnimSequenceBase> LandingAnimation = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/External/PolygonalCreaturesPack/PolygonalDragon/Animations/Polygonal_Dragon_AnimationFly_Down.Polygonal_Dragon_AnimationFly_Down")));

	UPROPERTY(EditAnywhere, Category="Animation", meta=(ClampMin="0.01"))
	float MontagePlayRate = 1.f;

	UPROPERTY(EditAnywhere, Category="Config", meta=(ClampMin="0.5"))
	float LandingTimeout = 4.f;

	float ElapsedTime = 0.f;

	UPROPERTY()
	TWeakObjectPtr<ARetrieveEnemyCharacter> CachedEnemy = nullptr;
};

USTRUCT(BlueprintType, meta=(DisplayName="Enemy Aerial Landing", Category="Retrieve|AI"))
struct RETRIEVE_API FStateTreeTask_EnemyAerialLanding : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeTask_EnemyAerialLandingInstanceData;

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

private:
	TStateTreeExternalDataHandle<APawn> PawnHandle;
};
