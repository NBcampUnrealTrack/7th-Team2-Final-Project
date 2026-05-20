#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "RetrievePawnData.generated.h"

class UInputMappingContext;
class URetrieveInputConfig;
class URetrieveAbilitySet;

UCLASS(BlueprintType, Const, Meta = (DisplayName = "Retrieve Pawn Data"))
class RETRIEVE_API URetrievePawnData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pawn")
	TSubclassOf<APawn> PawnClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pawn", meta = (Categories = "Pawn"))
	FGameplayTag PawnArchetypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	int32 DefaultMappingPriority = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<URetrieveInputConfig> InputConfig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
	TObjectPtr<URetrieveAbilitySet> DefaultAbilitySet;

	/**
	 * 비전투형 동료 Lumen(`PD_Lumen`)은 false로 설정합니다. 전투가 가능한 폰(`PD_Sovereign`,
	 * `PD_FieldMonster_*`, `PD_Boss_*`)은 true로 설정합니다.
	*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
	bool bRequiresAbilitySystem = true;

	/**
	 * 이 폰의 `MaxHealth` / `Health`를 초기화하는 데 사용할 `DT_CharacterStats` 행.
	 *   1. PawnData에 `CharacterStatsRow`가 설정되어 있으면 스켈레톤이 직접 사용합니다.
	 *   2. 적/보스 archetype은 이후 스프린트에서 자신의 `DT_MonsterData.StatsRow`를
	 *      읽어 컴포넌트에 푸시하는 방식으로 오버라이드합니다.
	 *
	 * `NAME_None`이면 AttributeSet의 컴파일 타임 기본값을 사용합니다.
	 *  — DataTable이 아직 작성되지 않은 초기 단계에 유용합니다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats")
	FName CharacterStatsRow;

	/** DataTable 에셋. `CharacterStatsRow` 설정 시 반드시 지정해야 합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats")
	TObjectPtr<UDataTable> CharacterStatsTable;
};
