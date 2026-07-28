#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Combat/RetrieveCombatTypes.h"
#include "RetrieveHitReactionProfile.generated.h"

class UAnimMontage;
class UGameplayEffect;

/**
 * 피격 반응 1종(Flinch/Stagger/Knockdown 등)의 연출 + 상태 정의
 * BlockingStateTags: 더 강한 상태가 active이면 약한 반응을 건너뛰도록 소유자별로 지정 (플레이어 State.Player.*, 적 State.Enemy.*)
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
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitReact|Blocking")
	FGameplayTagContainer BlockingStateTags;
};

/**
 * 캐릭터 피격 반응 프로파일
 */
UCLASS(BlueprintType)
class RETRIEVE_API URetrieveHitReactionProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitReact")
	TMap<ERetrieveHitReactType, FRetrieveHitReactionEntry> Reactions;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitReact|Cancel")
	FGameplayTagContainer AbilitiesToCancel;

	const FRetrieveHitReactionEntry* Find(ERetrieveHitReactType Type) const
	{
		return Reactions.Find(Type);
	}
};
