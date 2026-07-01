#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RetrieveUISettingsLibrary.generated.h"

class URetrieveUITheme;

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
};
