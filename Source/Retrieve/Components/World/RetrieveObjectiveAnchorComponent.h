#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "RetrieveObjectiveAnchorComponent.generated.h"

/**
 * 메인 퀘스트 목표의 월드 위치를 레벨에 표시하는 앵커.
 *
 * DT_Quest의 FQuestObjective에는 좌표가 없으므로, 목표가 가리키는 실제 대상
 * (수호자·제단·문 등)에 이 컴포넌트를 붙이고 그 목표의 CompletionTag를 지정한다.
 * QuestTrackerViewModel이 "현재 추적 중인 미완료 목표"의 태그를 마커 서브시스템에
 * 넘기면, 태그가 일치하는 이 앵커 위치에 화면 마커가 생긴다.
 *
 * 배치 주의:
 *   대상 액터의 Is Spatially Loaded 를 false 로 두는 것을 권장한다.
 *   World Partition으로 언로드된 동안에는 앵커도 사라져 마커가 끊기기 때문.
 *   (앵커가 다시 스트리밍되면 마커는 자동으로 복구된다)
 */
UCLASS(ClassGroup = (Retrieve), meta = (BlueprintSpawnableComponent, DisplayName = "Objective Anchor (퀘스트 목표 지점)"))
class RETRIEVE_API URetrieveObjectiveAnchorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URetrieveObjectiveAnchorComponent();

	/** 이 지점이 대응하는 목표의 CompletionTag(DT_Quest의 FQuestObjective와 동일). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker", meta = (Categories = "Quest.Step"))
	FGameplayTag ObjectiveStepTag;

	/** 비우면 DT_Quest의 목표 문구를 그대로 쓴다. 마커에만 다른 문구를 쓰고 싶을 때 지정. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker")
	FText MarkerLabelOverride;

	/** 액터 원점 기준 마커 오프셋. 기본값은 캐릭터 머리 위 정도. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker")
	FVector MarkerOffset = FVector(0.0f, 0.0f, 150.0f);

	/** 마커를 그릴 최종 월드 좌표. */
	UFUNCTION(BlueprintPure, Category = "Retrieve|ObjectiveMarker")
	FVector GetMarkerWorldLocation() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
