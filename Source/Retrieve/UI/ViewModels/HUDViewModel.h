#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "HUDViewModel.generated.h"

class UQuestTrackerViewModel;
class UElementGaugeViewModel;
class UBossStatusViewModel;
class UPlayerStatusViewModel;

/**
 * HUD 루트 ViewModel. 패널별 자식 VM을 UPROPERTY로 소유하여, HUD 위젯 인스턴스와 수명을 공유합니다.
 * ARetrievePlayerController::EnsureHUD에 의해 생성됩니다.
 */
UCLASS(BlueprintType)
class RETRIEVE_API UHUDViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UHUDViewModel();

	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|UI")
	UPlayerStatusViewModel* GetPlayerStatus() const { return PlayerStatus; }
	
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|UI")
	UElementGaugeViewModel* GetElementGauge() const { return ElementGauge; }
	
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|UI")
	UBossStatusViewModel* GetBossStatus() const { return BossStatus; }
	
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|UI")
	UQuestTrackerViewModel* GetQuestTracker() const { return QuestTracker; }

private:
	UPROPERTY()
	TObjectPtr<UPlayerStatusViewModel> PlayerStatus;

	UPROPERTY()
	TObjectPtr<UElementGaugeViewModel> ElementGauge;

	UPROPERTY()
	TObjectPtr<UBossStatusViewModel> BossStatus;

	UPROPERTY()
	TObjectPtr<UQuestTrackerViewModel> QuestTracker;
};
