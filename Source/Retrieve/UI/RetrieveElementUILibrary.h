#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "Data/RetrieveDataTableTypes.h"
#include "RetrieveElementUILibrary.generated.h"

class UDataTable;
class UImage;

/**
 * 원소 UI 전용 헬퍼 라이브러리.
 * MVVM View Bindings Conversion Function으로 사용된다.
 */
UCLASS()
class RETRIEVE_API URetrieveElementUILibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * GameplayTag(Element.Fire/Water/Wind) → LinearColor 변환.
	 * WBP_ElementGauge View Bindings의 FillColorAndOpacity Conversion Function으로 사용.
	 */
	UFUNCTION(BlueprintPure, Category = "Retrieve|UI|Element",
		meta = (DisplayName = "Element Tag To Color"))
	static FLinearColor ElementTagToColor(FGameplayTag ElementTag);

	/**
	 * SkillCombinationTable에서 ElementPattern에 매칭되는 첫 번째 FSkillCombination을 반환.
	 * WBP_BurstSkill에서 현재 게이지 상태 → 버스트 스킬 아이콘 조회에 사용.
	 * 매칭 행이 없거나 테이블이 null이면 false 반환.
	 *
	 * Blueprint 사용 예:
	 *   Pattern  = ElementGaugeComponent.GetCurrentCombination()
	 *   bFound, Combo = GetMatchingBurstCombination(DT_SkillCombinations, Pattern)
	 *   if bFound → GetDataTableRow(DT_BuffUIDefinitions, Combo.BurstUITag) → Row.Icon
	 */
	UFUNCTION(BlueprintPure, Category = "Retrieve|UI|Skill",
		meta = (DisplayName = "Get Matching Burst Combination"))
	static bool GetMatchingBurstCombination(
		const UDataTable* SkillCombinationTable,
		const TMap<FGameplayTag, int32>& ElementPattern,
		FSkillCombination& OutCombination);

	/**
	 * DT_BuffUIDefinitions에서 BuffUITag에 해당하는 FRetrieveBuffUIRow를 반환.
	 * Blueprint의 GetDataTableRow(wildcard) 대신 타입을 고정해 사용하기 위한 헬퍼.
	 * WBP_BurstSkill / WBP_AbsorbSkill에서 아이콘/틴트 조회 시 사용.
	 */
	UFUNCTION(BlueprintPure, Category = "Retrieve|UI|Buff",
		meta = (DisplayName = "Get Buff UI Row"))
	static bool GetBuffUIRow(
		const UDataTable* BuffUITable,
		FGameplayTag BuffUITag,
		FRetrieveBuffUIRow& OutRow);

	// Monolith 클래스 바인딩 우회용 헬퍼 — SetBrushFromTexture(Border)·SetColorAndOpacity(UserWidget) 대신 사용
	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI|Image",
		meta = (DisplayName = "Set Image Texture"))
	static void SetImageBrushTexture(UImage* Image, UTexture2D* Texture, bool bMatchSize = false);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI|Image",
		meta = (DisplayName = "Set Image Color"))
	static void SetImageColor(UImage* Image, FLinearColor Color);
};
