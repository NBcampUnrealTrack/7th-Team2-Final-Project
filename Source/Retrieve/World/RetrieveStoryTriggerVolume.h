#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"
#include "RetrieveStoryTriggerVolume.generated.h"

class UBoxComponent;

/**
 * 레벨에 배치하는 호스트 권한 스토리 트리거 볼륨.
 * 호스트 폰이 진입하면 설정에 따라 (1) 대상 NPC의 대화를 자동으로 열거나
 * (2) 퀘스트 스텝을 완료합니다. 둘 다 설정하면 둘 다 실행됩니다.
 * 스토리/퀘스트 진행 권한은 호스트에게만 있습니다(ABarkTriggerVolume과 같은
 * 볼륨 패턴이지만, 로컬이 아닌 호스트 권한으로 동작합니다).
 */
UCLASS()
class RETRIEVE_API ARetrieveStoryTriggerVolume : public AActor
{
	GENERATED_BODY()

public:
	ARetrieveStoryTriggerVolume();

	/**
	 * true(기본)면 CompleteStepTag가 설정된 볼륨이 자동으로 목표 마커 앵커가 된다.
	 * 즉 이 볼륨 위치가 "다음에 가야 할 곳"으로 화면/나침반/맵에 표시된다.
	 * 마커를 띄우고 싶지 않은 연출용 볼륨은 false로 끈다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Story|Marker")
	bool bCreateObjectiveAnchor = true;

	/** 마커를 볼륨 중심 기준으로 얼마나 띄울지. 큰 볼륨은 값을 키운다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Story|Marker")
	FVector ObjectiveAnchorOffset = FVector(0.0f, 0.0f, 150.0f);

	/** 목표 지점 베이크(URetrieveObjectiveAnchorDataAsset)가 읽어가는 접근자. */
	FGameplayTag GetObjectiveStepTag() const { return bCreateObjectiveAnchor ? CompleteStepTag : FGameplayTag(); }
	FVector GetObjectiveAnchorOffset() const { return ObjectiveAnchorOffset; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	                        int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);

	/** 진입 시 이 NPC의 URetrieveDialogueComponent 대화를 자동으로 엽니다(선택). 예: 동굴의 Lumen. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Retrieve|Story")
	TObjectPtr<AActor> DialogueTarget;

	/** 진입 시 완료할 퀘스트 스텝(선택). 예: Quest.Step.ExitedCave */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Story", meta = (Categories = "Quest.Step"))
	FGameplayTag CompleteStepTag;

	/** 진입 시(호스트 로컬) 순서대로 발동할 시스템 메시지 KeyTag들(선택).
	 *  각 태그를 RequestMessagesByKey로 발동 -> 같은 태그를 쓰는 자격 있는 행 전부가
	 *  Priority 오름차순(숫자가 작을수록 먼저)으로 큐에 쌓입니다.
	 *  (dismiss-required 행이면 Enter로 하나씩 넘기는 큐가 됩니다). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Story")
	TArray<FGameplayTag> SystemMessageKeysOnEnter;

	/** 진입 시(호스트 로컬) 순서대로 발동할 Bark KeyTag들(선택).
	 * 각 태그를 RequestBarkByKey로 개별 발동 -> BarkViewModel FIFO 큐에 순서대로 쌓여 한 줄씩 연속 재생됩니다.
	 * 각 대사는 고유 KeyTag를 가진 Manual 행이어야 합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Story")
	TArray<FGameplayTag> BarkKeysOnEnter;

	/** true면 1회만 발동합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Story")
	bool bOnce = true;

	UPROPERTY(VisibleAnywhere, Category = "Retrieve|Story")
	TObjectPtr<UBoxComponent> Box;

private:
	bool bFired = false;
};
