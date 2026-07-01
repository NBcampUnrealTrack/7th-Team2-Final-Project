#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "BarkStyleAsset.generated.h"

USTRUCT(BlueprintType)
struct FBarkStyleRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bark|Style")
	FLinearColor NameColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bark|Style")
	FLinearColor AccentColor = FLinearColor::White;
};

/** DA_BarkStyle: 스피커 태그별 자막 스타일. 스피커 추가 = 행 추가 */
UCLASS()
class RETRIEVE_API UBarkStyleAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bark|Style")
	TMap<FGameplayTag, FBarkStyleRow> Styles;

	bool GetStyle(FGameplayTag SpeakerTag, FBarkStyleRow& OutStyle) const
	{
		if (const FBarkStyleRow* Found = Styles.Find(SpeakerTag))
		{
			OutStyle = *Found;
			return true;
		}
		return false;
	}
};
