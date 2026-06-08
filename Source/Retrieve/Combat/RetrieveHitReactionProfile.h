#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Combat/RetrieveCombatTypes.h"
#include "RetrieveHitReactionProfile.generated.h"

class UAnimMontage;
class UGameplayEffect;

/**
 * 피격 반응 1종(Flinch/Stagger/Knockdown 등)의 연출 + 상태 정의
 */
USTRUCT(BlueprintType)
struct FRetrieveHitReactionEntry
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitReact")
	TSoftObjectPtr<UAnimMontage> Montage;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitReact")
	TSubclassOf<UGameplayEffect> StateEffect;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitReact")
	bool bCancelActions = false;
};

/**
 * 캐릭터 피격 반응 프로파일, 피격 반응 타입 → 연출/상태 매핑
 */
UCLASS(BlueprintType)
class RETRIEVE_API URetrieveHitReactionProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitReact")
	TMap<ERetrieveHitReactType, FRetrieveHitReactionEntry> Reactions;

	const FRetrieveHitReactionEntry* Find(ERetrieveHitReactType Type) const
	{
		return Reactions.Find(Type);
	}
};
