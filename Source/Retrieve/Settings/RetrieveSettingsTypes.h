#pragma once

#include "CoreMinimal.h"
#include "RetrieveSettingsTypes.generated.h"

/** 설정 화면의 좌측 카테고리. WidgetSwitcher 인덱스와 1:1 대응. */
UENUM(BlueprintType)
enum class ERetrieveSettingsCategory : uint8
{
	Graphics      UMETA(DisplayName = "Graphics"),
	Controls      UMETA(DisplayName = "Controls"),
	Audio         UMETA(DisplayName = "Audio"),
	Gameplay      UMETA(DisplayName = "Gameplay"),
	Accessibility UMETA(DisplayName = "Accessibility"),
	MAX           UMETA(Hidden)
};

/** 창 모드. UGameUserSettings의 EWindowMode를 BP 친화적으로 래핑. */
UENUM(BlueprintType)
enum class ERetrieveWindowMode : uint8
{
	Fullscreen         UMETA(DisplayName = "Fullscreen"),
	WindowedFullscreen UMETA(DisplayName = "Borderless"),
	Windowed           UMETA(DisplayName = "Windowed")
};

/** 색맹 보정 모드. 엔진 EColorVisionDeficiency로 매핑된다. */
UENUM(BlueprintType)
enum class ERetrieveColorBlindMode : uint8
{
	Off         UMETA(DisplayName = "Off"),
	Protanope   UMETA(DisplayName = "Protanopia (red)"),
	Deuteranope UMETA(DisplayName = "Deuteranopia (green)"),
	Tritanope   UMETA(DisplayName = "Tritanopia (blue)")
};

/** 오디오 채널. 각 채널은 RetrieveSettingsConfig의 SoundClass에 매핑된다. */
UENUM(BlueprintType)
enum class ERetrieveAudioChannel : uint8
{
	Master,
	Music,
	Sfx,
	Ambience,
	UI,
	Voice
};
