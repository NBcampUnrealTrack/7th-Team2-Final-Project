#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RetrieveSettingsConfig.generated.h"

class USoundMix;
class USoundClass;

/**
 * 설정 시스템의 프로젝트 단위 구성(Project Settings → Game → Retrieve Settings).
 * 오디오 볼륨을 적용할 SoundMix / 채널별 SoundClass와 게임플레이 범위 값을 보관한다.
 * 실제 사용자 값은 URetrieveGameUserSettings(ini)에 저장되고, 적용 대상 에셋만 여기서 지정한다.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Retrieve Settings"))
class RETRIEVE_API URetrieveSettingsConfig : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return FName(TEXT("Game")); }

	/** 볼륨 적용에 사용할 SoundMix. 비어 있으면 오디오 적용은 무시된다. */
	UPROPERTY(EditAnywhere, config, Category = "Audio")
	TSoftObjectPtr<USoundMix> SettingsSoundMix;

	UPROPERTY(EditAnywhere, config, Category = "Audio")
	TSoftObjectPtr<USoundClass> MasterSoundClass;

	UPROPERTY(EditAnywhere, config, Category = "Audio")
	TSoftObjectPtr<USoundClass> MusicSoundClass;

	UPROPERTY(EditAnywhere, config, Category = "Audio")
	TSoftObjectPtr<USoundClass> SfxSoundClass;

	UPROPERTY(EditAnywhere, config, Category = "Audio")
	TSoftObjectPtr<USoundClass> AmbienceSoundClass;

	UPROPERTY(EditAnywhere, config, Category = "Audio")
	TSoftObjectPtr<USoundClass> UISoundClass;

	UPROPERTY(EditAnywhere, config, Category = "Audio")
	TSoftObjectPtr<USoundClass> VoiceSoundClass;

	/** FOV 슬라이더 범위. */
	UPROPERTY(EditAnywhere, config, Category = "Gameplay", meta = (ClampMin = "60.0", ClampMax = "120.0"))
	float MinFieldOfView = 70.f;

	UPROPERTY(EditAnywhere, config, Category = "Gameplay", meta = (ClampMin = "60.0", ClampMax = "120.0"))
	float MaxFieldOfView = 110.f;

	/** UI/텍스트 스케일 슬라이더 범위. */
	UPROPERTY(EditAnywhere, config, Category = "Accessibility", meta = (ClampMin = "0.5", ClampMax = "2.0"))
	float MinUIScale = 0.8f;

	UPROPERTY(EditAnywhere, config, Category = "Accessibility", meta = (ClampMin = "0.5", ClampMax = "2.0"))
	float MaxUIScale = 1.5f;
};
