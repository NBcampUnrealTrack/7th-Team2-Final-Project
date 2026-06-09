#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "INotifyFieldValueChanged.h"
#include "RetrieveBossHPBarWidget.generated.h"

namespace UE::FieldNotification
{
	struct FFieldId;
}

class UBossStatusViewModel;
class UTextBlock;
class UWidget;

/**
 * Boss HP bar view.
 *
 * The owning HUD injects UHUDViewModel through the MVVM plugin. This widget then
 * registers the shared BossStatus ViewModel on its own UMVVMView and reacts to
 * FieldNotify updates from that ViewModel.
 */
UCLASS()
class RETRIEVE_API URetrieveBossHPBarWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

public:
	void SetBossStatusViewModel(UBossStatusViewModel* InViewModel);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_Name;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_HP;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> HUD_HealthBar_Enemy;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|Boss HP|Responsive")
	bool bAutoScaleWithViewport = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|Boss HP|Responsive", meta = (ClampMin = "1.0"))
	FVector2D DesignViewportSize = FVector2D(1920.f, 1080.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|Boss HP|Responsive", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float MinViewportScale = 0.55f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|Boss HP|Responsive", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float MaxViewportScale = 1.0f;

private:
	UPROPERTY()
	TObjectPtr<UBossStatusViewModel> BossStatusViewModel;

	TMap<FName, FDelegateHandle> BossFieldHandles;
	float LastAppliedViewportScale = -1.f;

	void BindToBossStatusViewModel();
	void BindToBossStatusViewModel(UBossStatusViewModel* InViewModel);
	void UnbindFromBossStatusViewModel();
	void RefreshFromViewModel();
	void HandleBossFieldChanged(UObject* Object, UE::FieldNotification::FFieldId FieldId);
	void SetFantasyHealthBarPercent(float Percent);
	void UpdateViewportScale();
	void AddBossFieldHandle(UE::FieldNotification::FFieldId FieldId, const INotifyFieldValueChanged::FFieldValueChangedDelegate& Delegate);

	static bool SetNumericWidgetProperty(UObject* Object, FName PropertyName, float Value);
	static bool CallFloatWidgetFunction(UObject* Object, FName FunctionName, float Value);
};
