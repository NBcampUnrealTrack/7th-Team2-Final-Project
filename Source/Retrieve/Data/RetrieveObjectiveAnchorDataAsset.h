#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "RetrieveObjectiveAnchorDataAsset.generated.h"

/** 레벨에서 구워둔 목표 지점 1개. */
USTRUCT(BlueprintType)
struct FRetrieveBakedObjectiveAnchor
{
	GENERATED_BODY()

	/** 이 지점이 대응하는 목표의 CompletionTag. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|ObjectiveMarker")
	FGameplayTag ObjectiveStepTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|ObjectiveMarker")
	FVector WorldLocation = FVector::ZeroVector;

	/** 구워질 당시의 소스 액터 이름(디버그/재베이크 확인용). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|ObjectiveMarker")
	FString SourceActorName;
};

/**
 * 메인 퀘스트 목표 지점을 레벨에서 미리 구워둔 에셋.
 *
 * 왜 필요한가: 앵커(URetrieveObjectiveAnchorComponent)와 스토리 볼륨은 World Partition으로
 * 스트리밍되므로, 아직 가보지 않은 먼 지역의 목표는 액터가 로드되기 전까지 위치를 알 수 없다.
 * 그래서 에디터에서 한 번 구워두고, 런타임에는 액터 로드 여부와 무관하게 이 좌표로 마커를 띄운다.
 * 실제 액터가 스트리밍되면 그쪽(살아있는 앵커)이 우선한다.
 *
 * 사용법:
 *   1. 에디터에서 목표가 있는 지역들을 모두 로드(World Partition 창에서 전체 로드 권장)
 *   2. 이 에셋을 열고 "Refresh From Level" 버튼 클릭 → 저장
 *   3. DA_MapConfig의 ObjectiveAnchorData 슬롯에 이 에셋을 할당
 *      (쿠킹 시 참조가 살아 있도록 반드시 연결할 것 — 참조가 없으면 패키지에서 빠진다)
 */
UCLASS(BlueprintType)
class RETRIEVE_API URetrieveObjectiveAnchorDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|ObjectiveMarker")
	TArray<FRetrieveBakedObjectiveAnchor> Anchors;

	/** 태그로 구워둔 위치를 찾는다. 없으면 false. */
	bool FindAnchorLocation(const FGameplayTag& StepTag, FVector& OutLocation) const;

#if WITH_EDITOR
	/**
	 * 현재 에디터 레벨에 로드된 액터들을 훑어 목표 지점을 다시 굽는다.
	 * 대상: URetrieveObjectiveAnchorComponent를 가진 액터 + CompleteStepTag가 설정된 스토리 볼륨.
	 * 로드되지 않은 지역의 액터는 잡히지 않으므로, 실행 전 지역을 모두 로드해야 한다.
	 */
	UFUNCTION(CallInEditor, Category = "Retrieve|ObjectiveMarker")
	void RefreshFromLevel();
#endif
};
