#pragma once

#include "CoreMinimal.h"
#include "Character/RetrieveCombatCharacter.h"
#include "EnemyCharacter.generated.h"

class URetrieveAbilitySystemComponent;
class UEnemyCombatComponent;
class UPatternCounterComponent;
class UDropComponent;

// 전방 선언만 남기고 헤더는 cpp에서 include
// "Components/EnemyCombatComponent.h"
// "Components/PatternCounterComponent.h"
// "Components/DropComponent.h"

UCLASS()
class RETRIEVE_API AEnemyCharacter : public ARetrieveCombatCharacter
{
	GENERATED_BODY()

public:
	AEnemyCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void BeginPlay() override;

	virtual void InitializeAbilitySystem() override;
	
	virtual void HandleDeathStarted(AActor* OwningActor) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|AbilitySystem")
	TObjectPtr<URetrieveAbilitySystemComponent> OwnedASC;

	/** 공격 패턴 선택·발동·쿨다운 담당 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<UEnemyCombatComponent> EnemyCombatComponent;

	/** 패턴 카운터 윈도우 추적 및 그로기 트리거. 에픽·보스에서 확장 재사용. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<UPatternCounterComponent> PatternCounterComponent;

	/** 사망 시 드랍 아이템 처리 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<UDropComponent> DropComponent;
};
