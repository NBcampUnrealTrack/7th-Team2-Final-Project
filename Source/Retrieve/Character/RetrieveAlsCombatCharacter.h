
#pragma once

#include "CoreMinimal.h"
#include "RetrieveAlsCharacter.h"
#include "RetrieveAlsCombatCharacter.generated.h"

class URetrieveHealthComponent;
/**
 * ARetrieveCombatCharacter의 ALS 가지 거울.
 * HealthComponent 보유 + ASC 초기화 시점에 Health<->ASC 연결.
 * 사망 시 기본 처리(이동 정지)는 베이스가 수행. 사망 GA 활성화/이벤트 전송은 아키타입(Sovereign 등)이 오버라이드.
 */
UCLASS()
class RETRIEVE_API ARetrieveAlsCombatCharacter : public ARetrieveAlsCharacter
{
	GENERATED_BODY()

public:
	ARetrieveAlsCombatCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	URetrieveHealthComponent* GetHealthComponent() const { return HealthComponent; }
	
	virtual void Revive(const FTransform& RespawnTransform);
	
protected:
	virtual void BeginPlay() override;

	void HandleAbilitySystemInitialized();

	/** 부활 후 "살아있는데 시체화된 상태"(래그돌/애님 정지/캡슐·이동 잠금/고아 LocomotionAction=입력 차단 잔존)를
	 * 점검·복구한다. Revive의 시점 분산 타이머 + SaveSubsystem::OnFastTravelCompleted(스트리밍 리스폰/빠른이동 완료)가
	 * 호출한다 — 고정 타이머만으로는 목적지 셀 스트리밍이 긴 리스폰(수 초~십수 초)에서 점검 창이 먼저 소진된다. */
	UFUNCTION()
	void ReassertAliveState();
	
	UFUNCTION()
	virtual void HandleDeathStarted(AActor* OwningActor);
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<URetrieveHealthComponent> HealthComponent;
};
