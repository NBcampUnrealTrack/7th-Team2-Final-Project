#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayTagContainer.h"
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
 * - 스폰/디스폰은 SpawnSphere(진입)·DespawnSphere(이탈) 콜리전으로 판정한다.
 * - 사망한 엔트리는 즉시 타이머를 걸지 않고, 플레이어가 DespawnSphere 밖으로
 *   나가는 시점부터 RespawnDelay를 카운트한다(거리 기준 리스폰).
 * - 카운트 도중 플레이어가 SpawnSphere 안으로 재진입하면 타이머를 취소한다.
 * - 화톳불 휴식(Channel.Player.Rested) 브로드캐스트를 받으면 거리 조건과 무관하게
 *   사망한 모든 엔트리를 즉시 리스폰한다.
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
	void TryRespawnEntry(int32 EntryIndex);

private:
	bool TryGetSpawnLocation(int32 EntryIndex, FVector& OutLocation) const;
	bool IsTriggerActor(const AActor* OtherActor) const;

	/** 트리거 액터(플레이어)가 현재 DespawnSphere 범위 안에 있는지. */
	bool IsPlayerInRange() const;

	/** EntryPawns 중 유효하고 사망하지 않은 스폰이 하나라도 있는지. false면 그룹이 전멸한 것. */
	bool HasAnyLiveSpawn() const;

	/** 트리거 액터(플레이어)가 현재 SpawnSphere 범위 안에 있는지 */
	bool IsPlayerInSpawnRange() const;

	/** 플레이어가 DespawnSphere 밖으로 나간 사망 엔트리들에 대해 RespawnDelay 타이머를 시작한다. */
	void StartPendingRespawnTimers();

	/** 화톳불 휴식 브로드캐스트 수신 시, 사망한 모든 엔트리를 거리 조건 없이 즉시 리스폰한다. */
	void ForceRespawnAllDeadEntries();

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

	/**
	 * (선택) 이 스포너가 이루는 스폰 그룹의 식별자.
	 * 지정할 경우 살아있는 스폰이 모두 사망할 때 Channel.Enemy.SpawnGroupCleared 신호에 이 값을 실어 발행한다.
	 * 퀘스트/구역 게이트 등 외부 시스템이 이 값으로 자신의 그룹을 매칭한다(스포너는 퀘스트에 의존하지 않음).
	 * 설정하지 않는 경우 소비자가 없으므로 신호를 발행하지 않는다.
	 */
	UPROPERTY(EditAnywhere, Category="Spawner")
	FGameplayTag SpawnGroupId;

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

	UPROPERTY(EditAnywhere, Category="Spawner|Test")
	bool bUseSpawnerLocationWhenSpawnPointMissing = false;

	UPROPERTY(VisibleAnywhere, Category="Spawner")
	TArray<TWeakObjectPtr<APawn>> SpawnedPawns;

	UPROPERTY(VisibleAnywhere, Category="Spawner")
	TArray<TWeakObjectPtr<APawn>> EntryPawns;
	
private:
	TArray<FTimerHandle> RespawnTimerHandles;

	FGameplayMessageListenerHandle RestListenerHandle;

	bool bIsSpawned = false;
};
