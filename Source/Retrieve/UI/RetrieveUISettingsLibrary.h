#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RetrieveUISettingsLibrary.generated.h"

class APlayerController;
class URetrieveUITheme;
class UUserWidget;

/**
 * UI 접근성 설정을 위젯이 한곳에서 읽도록 돕는 함수 라이브러리.
 * - 활성 테마(기본/고대비)를 설정에 따라 반환한다.
 * - UI 크기/모션 억제 질의를 제공한다.
 * 위젯은 하드코딩 색상 대신 GetActiveUITheme()의 색을 사용한다.
 */
UCLASS()
class RETRIEVE_API URetrieveUISettingsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** High Contrast 설정에 따라 기본/고대비 테마를 로드해 반환한다. 미설정 시 nullptr. */
	UFUNCTION(BlueprintPure, Category = "Retrieve|UI Settings")
	static URetrieveUITheme* GetActiveUITheme();

	/** 현재 UI 크기 배율(0.5~2.0). */
	UFUNCTION(BlueprintPure, Category = "Retrieve|UI Settings")
	static float GetUIScale();

	UFUNCTION(BlueprintPure, Category = "Retrieve|UI Settings")
	static bool IsHighContrastEnabled();

	UFUNCTION(BlueprintPure, Category = "Retrieve|UI Settings")
	static bool IsReduceMotionEnabled();

	/**
	 * "HUD 숨기기" 설정이 (확정 기준) 켜져 있는지. 상호작용 프롬프트·몬스터/보스 체력바 등
	 * 월드 공간 HUD 요소가 "기능은 유지하되 시각만 숨김"을 판정할 때 사용한다.
	 * (프리뷰가 아니라 Apply/Reset/취소로 확정된 값 — 서브시스템 IsHideHUDApplied. 미확보 시 저장값 폴백.)
	 */
	UFUNCTION(BlueprintPure, Category = "Retrieve|UI Settings", meta = (WorldContext = "WorldContextObject"))
	static bool IsHUDHidden(const UObject* WorldContextObject);

	/** 중첩된 자식 UserWidget 트리까지 들어가 모든 애니메이션을 정지한다(Reduce Motion 적용용). */
	static void StopAnimationsRecursive(UUserWidget* Root);

	/**
	 * 고대비 HUD를 현재 생성된 모든 UMG 위젯에 적용/해제한다.
	 * 켜면 모든 텍스트에 진한 그림자를 넣어 가독성을 높이고, 끄면 원래 그림자 상태로 복원한다.
	 * 설정 토글/Apply/부팅 시 서브시스템이 호출한다.
	 */
	static void ApplyHighContrastToAllWidgets(UObject* WorldContextObject);

	/**
	 * 위젯 1개 트리에만 고대비를 적용한다(설정이 켜져 있을 때만 동작).
	 * 고대비가 켜진 상태에서 새로 생성되는 패널의 NativeConstruct에서 호출한다.
	 */
	static void ApplyHighContrastToTree(UUserWidget* Root);

	/**
	 * WBP_ControlsGuide(조작키 안내) 전용 갱신. 그 위젯에만 존재하는 특정 이름의 키 텍스트
	 * TextBlock들을 찾아 현재 EnhancedInput 바인딩으로 텍스트를 갱신한다. GuideWidget에
	 * 해당 이름의 위젯이 없으면(다른 패널) 안전하게 아무 일도 하지 않는다.
	 * WBP_ControlsGuide의 Event Construct에서 Self를 넣어 호출한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI Settings")
	static void RefreshControlsGuideKeyLabels(UUserWidget* GuideWidget);

	/**
	 * 조작키 안내 다이어그램이 색+라벨로 표시할 수 있는 키보드 키인지.
	 * 설정 창 리바인드는 이 키들만 허용해 안내와 설정이 항상 일치하도록 한다.
	 * EnsureControlsGuideKeyCache로 캐시가 구축된 뒤에는 다이어그램에서 실제 발견된 키 기준으로,
	 * 그 전에는 알려진 기본 슬롯 목록 기준으로 판정한다. 시스템 예약 키(이동/인벤 등)는 항상 거부.
	 */
	UFUNCTION(BlueprintPure, Category = "Retrieve|UI Settings")
	static bool IsControlsGuideDisplayableKey(const FKey& Key);

	/**
	 * 조작키 안내 WBP를 임시 생성해 키보드 다이어그램의 표시 가능 키 집합을 캐시한다.
	 * 설정 창이 열릴 때 한 번 호출하면 리바인드 허용 키 판정이 실제 다이어그램 기준으로 동작한다.
	 */
	static void EnsureControlsGuideKeyCache(APlayerController* OwningPlayer);
};
