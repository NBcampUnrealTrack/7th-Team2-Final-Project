#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/RetrieveMapIconRegistry.h"
#include "RetrieveMapIconDataAsset.generated.h"

/**
 * 월드맵에 표시할 아이콘 항목 하나.
 * WP 로드 여부와 무관하게 항상 표시된다.
 */
USTRUCT(BlueprintType)
struct FRetrieveMapIconEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Icon")
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Icon")
	ERetrieveMapIconType IconType = ERetrieveMapIconType::POI;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Icon")
	FText MapLabel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Icon")
	bool bShowLabel = true;
};

/**
 * 월드맵 전용 정적 아이콘 목록 DataAsset.
 *
 * 사용법:
 *   1. 콘텐츠 브라우저에서 DA_WorldMapIcons (URetrieveMapIconDataAsset) 생성
 *   2. 에셋 열기 → 디테일 패널 → "Refresh From Level" 버튼 클릭
 *      → 현재 레벨에 배치된 모든 MapIconComponent 액터를 자동 스캔해 Icons 배열 채움
 *   3. 저장 후 WBP_WorldMap의 WorldMapIconData 슬롯에 이 에셋 할당
 *   4. 아이콘 추가/이동할 때마다 2번 반복
 */
UCLASS(BlueprintType)
class RETRIEVE_API URetrieveMapIconDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Icons")
	TArray<FRetrieveMapIconEntry> Icons;

#if WITH_EDITOR
	/** 현재 에디터 레벨에 배치된 모든 MapIconComponent 액터를 스캔해 Icons를 자동 갱신한다. */
	UFUNCTION(CallInEditor, Category="Retrieve|WorldMap")
	void RefreshFromLevel();
#endif
};
