#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "RetrievePawnData.generated.h"

class UGameplayEffect;
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
	 * 초기 스탯에 사용할 CharacterStatsTable의 행.
	 * NAME_None이거나 행을 찾지 못하면 FCharacterStats 기본값을 사용한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats")
	FName CharacterStatsRow;

	/** DataTable 에셋. `CharacterStatsRow` 설정 시 반드시 지정해야 합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats")
	TObjectPtr<UDataTable> CharacterStatsTable;

	/**
	 * true면 `CharacterStatsTable`을 `FLeveledCharacterStats` 테이블로 간주하고, 월드 레벨로 `ByLevel`을 인덱싱합니다.
	 * 플레이어·Lumen은 false(기존 flat `FCharacterStats` 경로 유지), 적/보스 archetype만 true로 설정합니다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats")
	bool bScalesWithWorldLevel = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats")
	TSubclassOf<UGameplayEffect> InitStatsEffect;
};
