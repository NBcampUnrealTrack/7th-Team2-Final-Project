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
	 * 원소 태그(Element.Fire/Water/Wind) → 흡수 버프 UI 태그(UI.Buff.Absorb.*).
	 * 정의되지 않은 원소면 빈 태그. GA_Absorb / 게이지 위젯의 흡수 아이콘 조회 공용.
	 */
	UFUNCTION(BlueprintPure, Category = "Retrieve|UI|Element",
		meta = (DisplayName = "Element Tag To Absorb Buff UI Tag"))
	static FGameplayTag ElementToAbsorbBuffUITag(FGameplayTag ElementTag);

	/**
	 * SkillCombinationTable에서 (WeaponType + BurstElement)가 일치하는 버스트 행을 반환.
	 * 버스트 스킬이 게이지 조합이 아닌 (무기 타입 × 현재 원소모드)로 결정되도록 바뀐 뒤의 조회용.
	 * GA_Burst::FindBurstForElement와 동일 기준. (원소는 항상 존재 — None 폴백 없음)
	 * HUD 버스트 아이콘 미리보기에서 사용.
	 */
	UFUNCTION(BlueprintPure, Category = "Retrieve|UI|Skill",
		meta = (DisplayName = "Get Burst Combination By Element"))
	static bool GetBurstCombinationByElement(
		const UDataTable* SkillCombinationTable,
		FGameplayTag WeaponType,
		FGameplayTag Element,
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
