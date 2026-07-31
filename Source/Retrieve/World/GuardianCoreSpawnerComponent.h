#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GuardianCoreSpawnerComponent.generated.h"

class AGuardianCoreActor;
struct FMonsterDiedPayload;

/**
 * 호스트 전용. Channel.Monster.Died를 구독하며, 처치된 보스가 가디언인 경우
 * 사망 위치에 해당 원소와 매칭되는 AGuardianCoreActor를 스폰합니다.
 * ARetrieveGameState의 기본 서브오브젝트로 생성됩니다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RETRIEVE_API UGuardianCoreSpawnerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGuardianCoreSpawnerComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void HandleMonsterDied(FGameplayTag Channel, const FMonsterDiedPayload& Message);

	UFUNCTION()
	void HandleSaveLoaded();

	/** 원소별 코어 BP. 각 BP의 CDO ElementTag를 통해 페이로드 원소와 매칭됩니다 */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|GuardianCore")
	TArray<TSubclassOf<AGuardianCoreActor>> GuardianCoreClasses;

	/** 사망 위치에 더해지는 Z 오프셋 (지면에서의 여유 높이). */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|GuardianCore")
	float SpawnZOffset = 0.f;

	FGameplayMessageListenerHandle MonsterDiedHandle;
};
