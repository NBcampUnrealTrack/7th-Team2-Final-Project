#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"
#include "GuardianCoreActor.generated.h"

class URetrieveInteractionResponseComponent;

/**
 * 수호자가 쓰러진 곳에 남기는 코어.
 * 플레이어가 UIM으로 상호작용 (홀드) → OnApplied (호스트 전용) → CompleteStep(GuardianDefeatedStep) → Destroy()
 * 원소 해방 자체는 루멘과의 상호작용 이후에 발생합니다.
 */
UCLASS()
class RETRIEVE_API AGuardianCoreActor : public AActor
{
	GENERATED_BODY()

public:
	AGuardianCoreActor();

	/** Spawner가 죽은 가디언의 원소와 매칭하기 위해 클래스 CDO에서 읽습니다. */
	UFUNCTION(BlueprintPure, Category = "Retrieve|GuardianCore")
	FGameplayTag GetElementTag() const { return ElementTag; }

	/** 상호작용 시 완료되는 단계 (Quest.Step.<E>GuardianDefeated). */
	UFUNCTION(BlueprintPure, Category = "Retrieve|GuardianCore")
	FGameplayTag GetGuardianDefeatedStep() const { return GuardianDefeatedStep; }

protected:
	virtual void BeginPlay() override;

	/** OnApplied (호스트 전용) 콜백: 단계를 완료하고 코어를 소멸합니다. */
	UFUNCTION()
	void HandleCoreInteracted(AActor* InteractionInstigator);

	UPROPERTY(VisibleAnywhere, Category = "Retrieve|GuardianCore")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Retrieve|GuardianCore")
	TObjectPtr<URetrieveInteractionResponseComponent> InteractionResponse;

	/** 이 코어가 대표하는 원소 (Element.Fire/Water/Wind). BP 기본값에서 설정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|GuardianCore", meta = (Categories = "Element"))
	FGameplayTag ElementTag;

	/** 상호작용 시 기록되는 단계 (Quest.Step.<E>GuardianDefeated). BP 기본값에서 설정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|GuardianCore", meta = (Categories = "Quest.Step"))
	FGameplayTag GuardianDefeatedStep;
};
