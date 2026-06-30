#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"
#include "SealGateActor.generated.h"

class URetrieveInteractionResponseComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRetrieveOnSealGateOpened);

/**
 * 성 지역의 출입을 제한하는 봉인 관문. 레벨에 배치되는 UIM 타겟.
 * Quest.Step.SealUnlocked가 완료된 경우에만 열 수 있습니다. 해당 퀘스트 스텝은 세 가지 원소를 강화 완료 했을 시 자동으로 완료됩니다.
 * 시각적 효과(문 이동 / 애니메이션 / VFX)는 OnGateOpened를 통해 BP 측에서 처리합니다.
 */
UCLASS()
class RETRIEVE_API ASealGateActor : public AActor
{
	GENERATED_BODY()

public:
	ASealGateActor();

	UFUNCTION(BlueprintPure, Category = "Retrieve|SealGate")
	bool IsOpened() const { return bOpened; }

	/** 관문이 열릴 때 한 번 발동됩니다 (호스트 OnApplied 또는 클라이언트 OnRep). BP 표현 훅. */
	UPROPERTY(BlueprintAssignable, Category = "Retrieve|SealGate")
	FRetrieveOnSealGateOpened OnGateOpened;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleInteracted(AActor* InteractionInstigator);

	UFUNCTION()
	void OnRep_bOpened();

	UPROPERTY(VisibleAnywhere, Category = "Retrieve|SealGate")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Retrieve|SealGate")
	TObjectPtr<URetrieveInteractionResponseComponent> InteractionResponse;

	/** 관문이 열리기 전 완료되어야 하는 단계 (기본값: Quest.Step.SealUnlocked). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|SealGate", meta = (Categories = "Quest.Step"))
	FGameplayTag RequiredStep;

	/** 개방 시 완료되는 단계 (기본값: Quest.Step.SealOpened). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|SealGate", meta = (Categories = "Quest.Step"))
	FGameplayTag OpenStep;

	UPROPERTY(ReplicatedUsing = OnRep_bOpened)
	bool bOpened = false;
};
