#pragma once

#include "CoreMinimal.h"
#include "UI/RetrieveElementAwareWidget.h"
#include "RetrieveElementSkillWidget.generated.h"

class UDataTable;
class UElementGaugeViewModel;
class UImage;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UWeaponComponent;
struct FRetrieveBuffUIRow;

/** 원소 흡수/버스트 스킬 아이콘을 표시하는 독립 HUD 위젯. */
UCLASS()
class RETRIEVE_API URetrieveElementSkillWidget : public URetrieveElementAwareWidget
{
	GENERATED_BODY()

public:
	URetrieveElementSkillWidget();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_AbsorbSkillIcon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_BurstSkillIcon;

	UPROPERTY(EditDefaultsOnly, Category = "ElementSkills|Data")
	TObjectPtr<UDataTable> BuffDefinitionTable;

	UPROPERTY(EditDefaultsOnly, Category = "ElementSkills|Data")
	TObjectPtr<UDataTable> SkillCombinationTable;

	UPROPERTY(EditDefaultsOnly, Category = "ElementSkills|Materials")
	TObjectPtr<UMaterialInterface> SkillIconMaskedMaterial;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnElementModeChanged(FGameplayTag NewElement) override;

private:
	TObjectPtr<UMaterialInstanceDynamic> AbsorbSkillIconMID;
	TObjectPtr<UMaterialInstanceDynamic> BurstSkillIconMID;
	TWeakObjectPtr<UElementGaugeViewModel> BoundViewModel;
	TWeakObjectPtr<UWeaponComponent> BoundWeaponComponent;

	void BindSources();
	void EnsureSkillIconTables();
	void UpdateSkillIcons();
	bool ResolveBurstBuffUIRow(FRetrieveBuffUIRow& OutRow) const;
	void ApplySkillIcon(UImage* Image, TObjectPtr<UMaterialInstanceDynamic>& IconMID, const FRetrieveBuffUIRow* Row, bool bEnabled);

	UFUNCTION()
	void HandleGaugeUpdated();

	UFUNCTION()
	void HandleWeaponChanged(FName WeaponItemId);
};
