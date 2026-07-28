#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RetrieveDataTableTool.generated.h"

class UDataTable;
class UGameplayEffect;
class URetrieveAbilitySet;

/**
 * DataTable 행의 특정 필드만 안전하게 일괄 수정하는 에디터 보조 유틸.
 * add_data_table_row 재추가 방식(행 전체 재작성)은 VisualParts 등 복합 필드 손상 위험이 있어,
 * RowMap에 직접 접근해 대상 필드 하나만 바꾼다. 에디터 Python에서 호출한다.
 */
UCLASS()
class RETRIEVE_API URetrieveDataTableTool : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** RowName이 RowPrefix로 시작하는 FRetrieveArmorDataRow 행들의 ArmorSetTag를 설정. 수정한 행 수 반환 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|DataTool")
	static int32 SetArmorSetTagByRowPrefix(UDataTable* Table, const FString& RowPrefix, FGameplayTag SetTag);

	/** RowName이 RowPrefix로 시작하는 FRetrieveWeaponDataRow 행들의 WeaponAbilitySet 경로를 설정. 수정한 행 수 반환 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|DataTool")
	static int32 SetWeaponAbilitySetByRowPrefix(UDataTable* Table, const FString& RowPrefix, const FString& AbilitySetObjectPath);

	/** AbilitySet의 GrantedGameplayEffects에 GE 추가. 이미 있으면 false */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|DataTool")
	static bool AddGrantedEffectToAbilitySet(URetrieveAbilitySet* AbilitySet, TSubclassOf<UGameplayEffect> EffectClass, float EffectLevel = 1.0f);
};
