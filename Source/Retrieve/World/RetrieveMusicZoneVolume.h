#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Settings/RetrieveMusicSettings.h"
#include "RetrieveMusicZoneVolume.generated.h"

class UBoxComponent;

/**
 * "이 구역 안에서는 이 BGM" 을 재생하는 영역(예: 성 지역). RetrieveWaterSuppressVolume과 동일한 골격.
 * 박스 안에 플레이어(UCombatStanceComponent 보유 액터)가 들어오면 존 트랙을 최우선으로 올리고,
 * 나가면 그 순간의 전역 상태(전투/기본)로 되돌린다.
 *
 * bUseCombatMusic을 켜면 이 구역 안에서도 전투가 벌어질 때 ZoneCombatBGM으로 전환된다.
 * 끄면 전투 여부와 무관하게 ZoneBGM만 재생한다(성 지역 등 '안전 구역').
 */
UCLASS()
class RETRIEVE_API ARetrieveMusicZoneVolume : public AActor
{
	GENERATED_BODY()

public:
	ARetrieveMusicZoneVolume();

	/**
	 * 현재 겹친 플레이어를 직접 확인해 진입/이탈을 동기화한다. 존 '안'에서 스폰/possess되어
	 * BeginOverlap 이벤트를 놓친 경우를 보정하기 위해 서브시스템이 InGame 진입 시 호출한다.
	 */
	void RefreshPlayerOverlap();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);
	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UPROPERTY(VisibleAnywhere, Category = "Retrieve|Music")
	TObjectPtr<UBoxComponent> Box;

	/** 이 구역의 평상시 BGM (예: 성 전용 트랙). */
	UPROPERTY(EditAnywhere, Category = "Retrieve|Music")
	FRetrieveMusicTrack ZoneBGM;

	/** 이 구역이 전투 시 별도의 전투곡을 쓸지. 끄면 전투 중에도 ZoneBGM 유지. */
	UPROPERTY(EditAnywhere, Category = "Retrieve|Music")
	bool bUseCombatMusic = false;

	/** bUseCombatMusic이 켜져 있을 때, 전투 중 재생할 이 구역 전용 전투곡. */
	UPROPERTY(EditAnywhere, Category = "Retrieve|Music", meta = (EditCondition = "bUseCombatMusic"))
	FRetrieveMusicTrack ZoneCombatBGM;

private:
	// 플레이어를 이 존 안에 있다고 처리했는지. 이벤트/직접 확인 양쪽에서 중복 진입·이탈을 막는다.
	bool bPlayerInside = false;
};
