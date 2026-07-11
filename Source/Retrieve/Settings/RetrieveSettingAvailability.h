#pragma once

#include "CoreMinimal.h"
#include "Settings/RetrieveSettingsTypes.h"

/**
 * 설정 옵션의 "실제 소비 구현" 여부 플래그.
 *
 * 소비 코드가 아직 없는 옵션은 false로 두어 설정 화면에서 행(Row_*)을 숨긴다.
 * 저장 프로퍼티(URetrieveGameUserSettings)는 향후 호환을 위해 그대로 둔다.
 * 기능이 구현되면(설계 문서 작업 6/10/11/13/14) 해당 플래그만 true로 바꾸면 다시 노출된다.
 */
namespace RetrieveSettingAvailability
{
	inline constexpr bool bAmbienceVolume     = true;  // 작업 6 완료: SC_Ambience + Weather 라우팅
	inline constexpr bool bVoiceVolume        = false; // 음성 콘텐츠 없음(추가 시 true)
	inline constexpr bool bSubtitles          = false; // 작업 14 자막 시스템 전
	inline constexpr bool bSubtitleScale      = false; // 작업 14
	inline constexpr bool bSubtitleBackground = false; // 작업 14
	inline constexpr bool bTutorialHints      = false; // 작업 14 튜토리얼 힌트 서비스 전
	inline constexpr bool bUIScale            = true;  // 작업 11: 전역 앱 스케일 적용
	inline constexpr bool bHighContrast       = true;  // 작업 11: 테마 교체(Default/HighContrast)
	inline constexpr bool bInteractMode       = false; // 작업 13 상호작용 토글/홀드 전
	inline constexpr bool bLockOnMode         = false; // 작업 10 락온 토글/홀드 전
	inline constexpr bool bAimAssist          = false; // 작업 14 Aim Assist 설계 전
	inline constexpr bool bGamepadOptions     = false; // 게임패드 미지원 빌드(패드 입력 미구현). 지원 시 true로 전환

	/** 페이지 내 옵션 행(Row_*) 1개에 대한 가용성 항목. */
	struct FOptionRow
	{
		ERetrieveSettingsCategory Category;
		const TCHAR* RowWidgetName;
		bool bAvailable;
	};

	/** 가용성에 따라 표시/숨김할 행 목록. 위젯명은 페이지 WBP의 Row_* 컨테이너 이름. */
	inline TArray<FOptionRow> GetOptionRows()
	{
		return {
			{ ERetrieveSettingsCategory::Audio,         TEXT("Row_Ambience"),      bAmbienceVolume },
			{ ERetrieveSettingsCategory::Audio,         TEXT("Row_Voice"),         bVoiceVolume },
			{ ERetrieveSettingsCategory::Gameplay,      TEXT("Row_Subtitles"),     bSubtitles },
			{ ERetrieveSettingsCategory::Gameplay,      TEXT("Row_SubtitleScale"), bSubtitleScale },
			{ ERetrieveSettingsCategory::Gameplay,      TEXT("Row_TutorialHints"), bTutorialHints },
			{ ERetrieveSettingsCategory::Controls,      TEXT("Row_LockOn"),        bLockOnMode },
			{ ERetrieveSettingsCategory::Controls,      TEXT("Row_PadSens"),       bGamepadOptions },
			{ ERetrieveSettingsCategory::Controls,      TEXT("Row_Vibration"),     bGamepadOptions },
			{ ERetrieveSettingsCategory::Accessibility, TEXT("Row_UIScale"),       bUIScale },
			{ ERetrieveSettingsCategory::Accessibility, TEXT("Row_HighContrast"),  bHighContrast },
			{ ERetrieveSettingsCategory::Accessibility, TEXT("Row_Interact"),      bInteractMode },
			{ ERetrieveSettingsCategory::Accessibility, TEXT("Row_AimAssist"),     bAimAssist },
			{ ERetrieveSettingsCategory::Accessibility, TEXT("Row_SubtitleBG"),    bSubtitleBackground },
		};
	}

	/** 한 섹션(헤더 + 앞 구분선 + 행들). 섹션 내 모든 행이 숨겨지면 헤더/구분선도 숨긴다. */
	struct FOptionSection
	{
		ERetrieveSettingsCategory Category;
		const TCHAR* HeaderWidgetName;    // 예: "Hdr_Input"
		const TCHAR* DividerWidgetName;   // 헤더 바로 앞 구분선. 없으면 nullptr.
		TArray<const TCHAR*> RowWidgetNames;
	};

	/** 모든 행이 unavailable이면 헤더/구분선까지 숨길 섹션 목록. */
	inline TArray<FOptionSection> GetOptionSections()
	{
		return {
			{ ERetrieveSettingsCategory::Accessibility, TEXT("Hdr_Input"), TEXT("SizeBox"),
				{ TEXT("Row_Interact"), TEXT("Row_AimAssist") } },
		};
	}
}
