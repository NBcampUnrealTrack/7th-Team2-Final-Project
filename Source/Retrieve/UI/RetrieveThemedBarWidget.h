#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Settings/RetrieveSettingsTypes.h"
#include "RetrieveThemedBarWidget.generated.h"

class UProgressBar;

/** 이 바가 어떤 테마 색을 쓸지. */
UENUM(BlueprintType)
enum class ERetrieveBarColorRole : uint8
{
	Health,
	Stamina
};

/**
 * 게이지(ProgressBar) 색을 접근성 테마(고대비)에 맞춰 교체하는 HUD 바 베이스.
 * 중첩된 팩 위젯 안의 ProgressBar까지 재귀로 찾아 High Contrast 시 테마 색으로,
 * 끄면 원본 색으로 복원한다(기본 룩 보존).
 * WBP_HPBar / WBP_Stamina를 이 클래스로 리페어런트해 사용한다.
 */
UCLASS(Abstract, Blueprintable)
class RETRIEVE_API URetrieveThemedBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Health=체력 색, Stamina=스태미나 색. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Theme")
	ERetrieveBarColorRole BarColorRole = ERetrieveBarColorRole::Health;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** 접근성 설정(고대비) 변경 시 즉시 다시 적용. */
	UFUNCTION() void HandleSettingChanged(ERetrieveSettingsCategory Category);

	/** 현재 테마/고대비 상태에 맞춰 게이지 색을 적용한다. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Theme")
	void ApplyThemeToBars();

	/** Reduce Motion이 켜져 있으면 중첩 트리의 모든 UMG 애니메이션(장식용 연출)을 정지한다. */
	void ApplyReduceMotion();

private:
	/** 원본 색 캐시(고대비 해제 시 복원용). */
	UPROPERTY()
	TMap<TObjectPtr<UProgressBar>, FLinearColor> OriginalFillColors;

	bool bCachedOriginals = false;
};
