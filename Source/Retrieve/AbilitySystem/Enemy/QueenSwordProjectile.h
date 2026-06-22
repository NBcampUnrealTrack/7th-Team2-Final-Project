
#pragma once

#include "CoreMinimal.h"
#include "EnemyProjectile.h"
#include "QueenSwordProjectile.generated.h"

class USceneComponent;

UCLASS(Blueprintable, BlueprintType)
class RETRIEVE_API AQueenSwordProjectile : public AEnemyProjectile
{
	GENERATED_BODY()

public:
	AQueenSwordProjectile();
	/** 이동과 충돌을 끄고 FollowComponent를 따라다니게 함. */
	UFUNCTION(BlueprintCallable, Category="Queen|Sword Projectile")
	void PrepareProjectile(USceneComponent* FollowComponent);

	/** 부착을 해제하고 현재 타깃 위치로 발사합니다. */
	UFUNCTION(BlueprintCallable, Category="Queen|Sword Projectile")
	bool FireAtTarget(
		AActor* TargetActor,
		float Speed,
		float Lifetime,
		FVector TargetOffset);

	UFUNCTION(BlueprintPure, Category="Queen|Sword Projectile")
	bool IsPrepared() const { return bPrepared; }
	
private:
	bool bPrepared = false;
	bool bLaunched = false;
};
