#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "RetrieveCosmeticAnimationTypes.generated.h"

class UAnimInstance;

/**
 * 태그 조건을 만족했을 때 적용할 Linked Anim Layer 클래스입니다.
 *
 * 성별, 무기, 스탠스, 원소처럼 애니메이션을 바꾸는 축은 GameplayTag로 표현합니다.
 * 예: Gender.Female + Weapon.Staff + Stance.Combat
 */
USTRUCT(BlueprintType)
struct FRetrieveAnimLayerSelectionEntry
{
	GENERATED_BODY()

	/** RequiredTags를 모두 만족했을 때 적용할 Anim Layer 클래스입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UAnimInstance> Layer;

	/** 현재 태그 컨테이너가 이 태그들을 모두 가지고 있어야 합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTagContainer RequiredTags;
};

/**
 * 현재 태그 상태에 맞는 Linked Anim Layer 클래스를 선택합니다.
 *
 * LayerRules는 위에서 아래로 평가됩니다.
 * 더 구체적인 조합일수록 앞에 배치해야 합니다.
 *
 * 예:
 * 1. Gender.Female + Weapon.Staff + Stance.Combat
 * 2. Weapon.Staff + Stance.Combat
 * 3. Weapon.Staff
 */
USTRUCT(BlueprintType)
struct FRetrieveAnimLayerSelectionSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (TitleProperty = Layer))
	TArray<FRetrieveAnimLayerSelectionEntry> LayerRules;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UAnimInstance> DefaultLayer;

	TSubclassOf<UAnimInstance> SelectBestLayer(const FGameplayTagContainer& Tags) const;
};
