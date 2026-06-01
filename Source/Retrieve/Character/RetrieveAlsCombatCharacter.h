
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
	
protected:
	virtual void BeginPlay() override;
	
	void HandleAbilitySystemInitialized();
	
	UFUNCTION()
	virtual void HandleDeathStarted(AActor* OwningActor);
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<URetrieveHealthComponent> HealthComponent;
};
