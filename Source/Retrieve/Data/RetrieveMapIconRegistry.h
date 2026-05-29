#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "RetrieveMapIconRegistry.generated.h"

class UTexture2D;

/**
 * DT_MapIconRegistry DataTable의 Row 구조체.
 * RowName 규약: Enum 이름과 동일 (예: "POI", "Boss", "Outpost")
 */
USTRUCT(BlueprintType)
struct RETRIEVE_API FRetrieveMapIconRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UTexture2D> IconTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor IconColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="8", UIMin="8"))
	float IconSize = 16.0f;
};

UENUM(BlueprintType)
enum class ERetrieveMapIconType : uint8
{
	None     UMETA(DisplayName="None"),
	Player   UMETA(DisplayName="Player"),
	POI      UMETA(DisplayName="POI"),
	Lumen    UMETA(DisplayName="Lumen"),
	Boss     UMETA(DisplayName="Boss"),
	Outpost  UMETA(DisplayName="Outpost"),
	FirstWeapon  UMETA(DisplayName="FirstWeapon"),
};

/**
 * DT_MapIconRegistry DataTable을 참조해 ERetrieveMapIconType → FRetrieveMapIconRow를 조회한다.
 *
 * 사용법:
 *   1. Content Browser에서 DT_MapIconRegistry DataTable 생성 (Row Structure: FRetrieveMapIconRow)
 *   2. WBP_Minimap에서 IconRegistry 오브젝트의 IconDataTable에 DT_MapIconRegistry 할당
 *   3. 각 Row 이름을 ERetrieveMapIconType 이름과 동일하게 설정 (POI, Boss 등)
 */
UCLASS(BlueprintType, Blueprintable)
class RETRIEVE_API URetrieveMapIconRegistry : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Minimap")
	TObjectPtr<UDataTable> IconDataTable;

	// 등록되지 않은 타입에 쓸 폴백
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Minimap")
	FRetrieveMapIconRow FallbackRow;

	const FRetrieveMapIconRow& FindRow(ERetrieveMapIconType IconType) const;

private:
	static FName IconTypeToRowName(ERetrieveMapIconType IconType);
};
