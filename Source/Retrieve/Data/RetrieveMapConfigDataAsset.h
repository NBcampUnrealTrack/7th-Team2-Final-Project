#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RetrieveMapConfigDataAsset.generated.h"

class UTexture2D;
class URetrieveMapIconDataAsset;
class URetrieveMapIconRegistry;
class URetrieveObjectiveAnchorDataAsset;

UCLASS(BlueprintType)
class RETRIEVE_API URetrieveMapConfigDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Map")
	TObjectPtr<UTexture2D> BakedMapTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Map")
	FVector2D MapOrigin = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Map")
	FVector2D MapExtentXY = FVector2D(1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Map")
	TObjectPtr<URetrieveMapIconDataAsset> WorldMapIconData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Map")
	TObjectPtr<URetrieveMapIconRegistry> IconRegistry;

	/**
	 * 레벨에서 구워둔 메인 퀘스트 목표 지점들.
	 * 여기에 연결해 두면 목표 액터가 World Partition으로 언로드된 상태에서도
	 * 마커가 항상 표시되고, 쿠킹 시 이 참조 덕분에 에셋이 패키지에 포함된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Map")
	TObjectPtr<URetrieveObjectiveAnchorDataAsset> ObjectiveAnchorData;
};