#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RetrieveMusicSettings.generated.h"

class USoundBase;

/**
 * BGM 한 트랙 = 사운드 + 볼륨 배수. 트랙마다 녹음 음량이 달라 생기는 편차를
 * 에디터에서 보정하기 위한 배수(옵션의 '음악 볼륨' 슬라이더가 그 위에 전체 배수로 곱해진다).
 */
USTRUCT(BlueprintType)
struct FRetrieveMusicTrack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music")
	TSoftObjectPtr<USoundBase> Sound;

	/** 이 트랙의 기준 볼륨 배수. 1.0 = 원음. 작은 파일을 키우려면 1.0 이상도 허용. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float Volume = 1.f;
};

/**
 * 배경음악(BGM) 전역 설정. Project Settings > Retrieve > Retrieve Music.
 * 기본/전투 BGM은 게임 전체 공용 1쌍이며, 지역 전용 BGM은 RetrieveMusicZoneVolume에서 개별 지정한다.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Retrieve Music"))
class RETRIEVE_API URetrieveMusicSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Retrieve"); }

	/** 메인 메뉴 BGM (세션 상태가 MainMenu일 때). */
	UPROPERTY(Config, EditAnywhere, Category = "Music")
	FRetrieveMusicTrack MenuBGM;

	/** 결과 화면 BGM (세션 상태가 Result일 때). */
	UPROPERTY(Config, EditAnywhere, Category = "Music")
	FRetrieveMusicTrack ResultBGM;

	/** 평상시(비전투) BGM. */
	UPROPERTY(Config, EditAnywhere, Category = "Music")
	FRetrieveMusicTrack DefaultBGM;

	/** 전투 중(추적하는 적이 있을 때) BGM. */
	UPROPERTY(Config, EditAnywhere, Category = "Music")
	FRetrieveMusicTrack CombatBGM;

	/** 트랙 전환 크로스페이드 시간(초). */
	UPROPERTY(Config, EditAnywhere, Category = "Music", meta = (ClampMin = "0.0"))
	float CrossfadeDuration = 1.5f;

	/** 곡이 끝난 뒤 다시 재생하기까지 두는 간격(초). 0이면 곡 사이 텀 없이 이어 반복. */
	UPROPERTY(Config, EditAnywhere, Category = "Music", meta = (ClampMin = "0.0"))
	float LoopGapSeconds = 3.f;
};
