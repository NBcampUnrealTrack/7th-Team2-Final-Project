#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "BarkSpeakerComponent.generated.h"

struct FRetrieveBarkPayload;

/**
 * Bark(주변에서 들려오는 대사)를 말할 수 있는 액터(e.g. Lumen, 퀘스트 NPC, 마을 사람)에 붙이는 컴포넌트.
 * (1) "나는 이런 스피커다"라는 정보(SpeakerTag, 근접 거리, 발화 간격)를 들고 있고,
 * (2) BarkSubsystem이 나를 발화자로 고르면, 고른 대사를 화면(자막)으로 전달합니다.
 * 
 * Lumen은 특별 취급이 아닌 "항상 근접 거리 안"(Range<=0)인 스피커입니다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RETRIEVE_API UBarkSpeakerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBarkSpeakerComponent();

	/**
	 * 이 스피커가 누구인지 나타내는 태그(e.g. Speaker.Lumen / Speaker.NPC.<id>).
	 * DT_Bark 행의 SpeakerTag와 같아야 해당 행을 말할 수 있고, DA_BarkStyle도 이 태그로 찾습니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bark")
	FGameplayTag SpeakerTag;

	/** 로컬 플레이어와의 근접 사정거리(cm). 이 거리 안에 들어와야 발화 후보가 됩니다. 0 이하 = 항상 사정거리 안(동행자 Lumen). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bark", meta = (ClampMin = "0.0"))
	float Range = 0.f;

	/** 발화 간 최소 간격(초). 같은 NPC가 쉴 새 없이 떠드는 걸 막습니다. (OnQuestStep·Manual 대사에는 적용되지 않음) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bark", meta = (ClampMin = "0.0"))
	float MinInterval = 10.f;

	/**
	 * 코옵용 플래그(지금은 동작 없음). 켜면 추후 호스트가 이 발화를 모든 클라이언트에 동기화해서,
	 * Lumen의 대사를 모두가 함께 듣게 됩니다. 싱글플레이에서 켜도 무해합니다. Lumen은 true, 일반 NPC는 false.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bark")
	bool bSynchronized = false;

	/**
	 * BarkSubsystem이 이 스피커를 발화 대상으로 골랐을 때 호출합니다.
	 * 받은 페이로드를 Channel.UI.BarkRequested로 로컬 발행 -> BarkViewModel이 받아 자막을 띄웁니다.
	 */
	void RouteBark(const FRetrieveBarkPayload& Payload);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
