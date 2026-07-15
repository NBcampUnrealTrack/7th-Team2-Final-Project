#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UI/RetrieveGamePanelWidget.h"
#include "RetrieveSkillOverviewWidget.generated.h"

class UTextBlock;
class UWeaponComponent;
class UArmorComponent;
class UElementResonanceComponent;

/**
 * 현재 장비와 원소 조합으로 사용할 수 있는 흡수/버스트/세트/공명 효과를 설명하는 패널.
 * 장비 및 공명 델리게이트와 짧은 주기 갱신을 함께 사용해 표시 내용을 현재 상태와 동기화한다.
 *
 * WBP 필수 이름:
 * Text_WeaponSection / Text_SkillSection / Text_SetSection /
 * Text_ResonanceSection / Text_AdvantageSection
 */
UCLASS()
class RETRIEVE_API URetrieveSkillOverviewWidget : public URetrieveGamePanelWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Skill", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_WeaponSection;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Skill", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_SkillSection;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Skill", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_SkillFire;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Skill", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_SkillWater;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Skill", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_SkillWind;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Skill", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_SetSection;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Skill", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ResonanceSection;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Skill", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_AdvantageSection;

	/** 패널이 열린 동안 원소 스택처럼 별도 이벤트가 없는 값도 동기화한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Skill", meta = (ClampMin = "0.1"))
	float RefreshInterval = 0.5f;

private:
	UFUNCTION() void HandleWeaponChanged(FName WeaponItemId);
	UFUNCTION() void HandleArmorChanged(FGameplayTag EquipmentSlotTag, FName ArmorItemId);
	UFUNCTION() void HandleResonanceChanged();
	void HandleTimerRefresh();

	void RefreshAll();
	FText BuildWeaponSection() const;
	FText BuildSkillSection(const FGameplayTag& ElementTag) const;
	FText BuildSetSection() const;
	FText BuildResonanceSection() const;
	FText BuildAdvantageSection() const;

	FGameplayTag GetCurrentElementTag() const;
	static FString ElementTagToKorean(const FGameplayTag& ElementTag);

	UWeaponComponent* GetWeaponComponent() const;
	UArmorComponent* GetArmorComponent() const;
	UElementResonanceComponent* GetResonanceComponent() const;

	FTimerHandle RefreshTimerHandle;
};

