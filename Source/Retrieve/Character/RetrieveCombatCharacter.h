#pragma once

#include "CoreMinimal.h"
#include "Character/RetrieveCharacter.h"
#include "RetrieveCombatCharacter.generated.h"

class URetrieveHealthComponent;

/**
 * 전투 가능한 폰(소버린, 적, 보스)에 UHealthComponent를 추가하는 베이스 클래스.
 * Lumen은 비전투형 캐릭터이므로 ARetrieveCharacter를 직접 상속합니다.
 */
UCLASS()
class RETRIEVE_API ARetrieveCombatCharacter : public ARetrieveCharacter
{
	GENERATED_BODY()

public:
	ARetrieveCombatCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	URetrieveHealthComponent* GetHealthComponent() const { return HealthComponent; }

protected:
	virtual void BeginPlay() override;

	/** PawnExtension의 OnAbilitySystemInitialized에 등록됨. HealthComponent를 ASC와 연결합니다. */
	void HandleAbilitySystemInitialized();

	/** URetrieveHealthComponent::OnDeathStarted에 구독됨. 기본 구현은 비어 있으며 아키타입이 오버라이드합니다. */
	UFUNCTION()
	virtual void HandleDeathStarted(AActor* OwningActor);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<URetrieveHealthComponent> HealthComponent;
};
