#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpawnerBase.generated.h"

class URetrievePawnData;
class USphereComponent;

/**
 * 스폰 엔트리: PawnData(스폰 클래스)와 스포너 기준 상대 위치를 1:1로 매핑한다.
 * 랜덤 배치·수량 등 확장이 필요하면 파생 구조체를 사용할 것.
 */
USTRUCT(BlueprintType)
struct FSpawnEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn")
	TObjectPtr<URetrievePawnData> PawnData = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn")
	TObjectPtr<AActor> SpawnPoint = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn")
	bool bIsRespawnable = false;
};

/**
 * 거리 기반 스폰/디스폰과 리스폰 로직의 기반 클래스.
 *
 * - 거리 체크(CheckDistance)는 1.5s 타이머로 폴링한다.
 * - 전원 사망 감지는 파생 클래스에서 NotifyPawnDied()를 호출해 위임한다.
 * - 리스폰은 플레이어 시야 이탈 확인(IsSpawnLocationHidden) 후 SpawnAll을 실행한다.
 *
 * 랜덤 배치 등 확장은 SpawnAll을 override해 구현한다.
 */
UCLASS(Abstract)
class RETRIEVE_API ASpawnerBase : public AActor
{
	GENERATED_BODY()

public:
	ASpawnerBase();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void SpawnAll();
	virtual void DespawnAll();
	// virtual void TryRespawn();
	void TryRespawnEntry(int32 EntryIndex);

private:
	bool IsPositionHidden(const FVector& WorldPos) const;
	
	UFUNCTION()
	void OnSpawnSphereBeginOverlap(UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnDespawnSphereEndOverlap(UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);
	
	UFUNCTION()
	void OnEnemyDeath(AActor* Actor);
	
public:
	UPROPERTY(EditAnywhere, Category="Spawner")
	bool bAllowRespawn = true;
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Spawner")
	TObjectPtr<USceneComponent> RootComp;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Spawner")
	TObjectPtr<USphereComponent> SpawnSphereComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Spawner")
	TObjectPtr<USphereComponent> DespawnSphereComp;
	
	UPROPERTY(EditAnywhere, Category="Spawner")
	TSubclassOf<AActor> TriggerActorClass;
	
	UPROPERTY(EditAnywhere, Category="Spawner")
	float RespawnDelay = 10.f;

	UPROPERTY(EditAnywhere, Category="Spawner")
	TArray<FSpawnEntry> SpawnList;

	UPROPERTY(VisibleAnywhere, Category="Spawner")
	TArray<TWeakObjectPtr<APawn>> SpawnedPawns;

	UPROPERTY(VisibleAnywhere, Category="Spawner")
	TArray<TWeakObjectPtr<APawn>> EntryPawns;
	
private:
	TArray<FTimerHandle> RespawnTimerHandles; 

	bool bIsSpawned = false;
};
