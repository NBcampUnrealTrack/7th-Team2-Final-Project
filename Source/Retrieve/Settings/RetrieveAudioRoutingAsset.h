#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "Settings/RetrieveSettingsTypes.h"
#include "RetrieveAudioRoutingAsset.generated.h"

class USoundBase;

/**
 * 사운드 묶음을 한 오디오 채널(SoundClass)로 라우팅하는 규칙 1건.
 * 폴더 단위(하위 포함) 또는 개별 사운드로 지정한다.
 */
USTRUCT(BlueprintType)
struct FRetrieveAudioRoutingRule
{
	GENERATED_BODY()

	/** 이 규칙이 적용할 오디오 채널. RetrieveSettingsConfig의 SoundClass에 매핑된다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio Routing")
	ERetrieveAudioChannel Channel = ERetrieveAudioChannel::Sfx;

	/** 이 콘텐츠 폴더(하위 포함) 아래 모든 사운드에 적용. 예: /Game/Retrieve/Audio/SFX */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio Routing", meta = (ContentDir))
	TArray<FDirectoryPath> Folders;

	/** 개별 사운드 명시 지정(폴더 규칙과 함께 적용 가능). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio Routing", meta = (AllowedClasses = "/Script/Engine.SoundBase"))
	TArray<TSoftObjectPtr<USoundBase>> Sounds;
};

/**
 * 사운드 → 오디오 채널(SoundClass) 분류를 한곳에서 관리하는 데이터에셋.
 *
 * 각 사운드 에셋의 SoundClass를 일일이 지정하는 대신, 이 에셋의 규칙을 런타임에 읽어
 * 매칭되는 사운드의 SoundClassObject를 메모리에서 지정한다(.uasset 미수정 → LFS 충돌 없음).
 * URetrieveSettingsSubsystem::ApplyAudioRouting()이 소비한다.
 */
UCLASS(BlueprintType)
class RETRIEVE_API URetrieveAudioRoutingAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio Routing")
	TArray<FRetrieveAudioRoutingRule> Rules;

#if WITH_EDITOR
	/**
	 * [에디터 버튼] 규칙대로 매칭되는 모든 사운드의 SoundClass를 에셋에 지정하고 저장한다.
	 * 한 번 클릭으로 일괄 할당 — 런타임 로드 비용 없이 재생 시 자동으로 채널을 따른다.
	 * 같은 사운드가 여러 규칙에 걸리면 배열에서 뒤에 있는 규칙이 우선(last-write-wins).
	 */
	UFUNCTION(CallInEditor, Category = "Audio Routing")
	void ApplyRoutingToAssets();
#endif
};
