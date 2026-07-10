#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "TimerManager.h"
#include "Settings/RetrieveMusicSettings.h"
#include "Core/RetrieveSessionState.h"
#include "RetrieveMusicSubsystem.generated.h"

class UAudioComponent;
class USoundBase;
class ARetrieveMusicZoneVolume;
struct FEnemyPlayerSpottedPayload;
struct FRetrieveSessionStatePayload;
struct FPlayerDiedPayload;

/**
 * BGM 중앙 관리자. 두 개의 UAudioComponent 슬롯을 번갈아 크로스페이드한다.
 * 우선순위: 지역(존) BGM > 전투 BGM > 기본 BGM. 각 트랙은 자체 볼륨 배수를 가진다.
 *
 * 전투 BGM 판정:
 *  - 진입: Channel.Enemy.PlayerSpotted (로컬 플레이어가 대상일 때) — 나를 포착한 몬스터를 교전 집합에 추가.
 *          공격/무기 장착만으로는 진입하지 않는다.
 *  - 이탈: 교전 집합이 비는 순간. 폴링으로 죽음(Dead)/비활성(Idle)/추적 포기 복귀(Return) 상태인
 *          몬스터를 걷어내 "교전 중인 몬스터가 하나도 없을 때" 기본 BGM으로 복귀한다.
 *
 * 지역(존): RetrieveMusicZoneVolume이 진입 시 자기 트랙 세트를 넘긴다.
 *  - bUseCombatMusic=false: 전투 여부와 무관하게 ZoneBGM만 (성 지역 등).
 *  - bUseCombatMusic=true : 전투 중이면 ZoneCombatBGM, 아니면 ZoneBGM.
 */
UCLASS()
class RETRIEVE_API URetrieveMusicSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** PlayerSpotted 필터에 쓸 로컬 플레이어 폰 등록 (SovereignCharacter::InitializeAbilitySystem). */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Music")
	void RegisterPlayer(APawn* PlayerPawn);

	/** 존 볼륨 진입 (RetrieveMusicZoneVolume에서 호출). 존 안에서는 존 트랙 세트가 최우선. */
	void EnterMusicZone(const FRetrieveMusicTrack& ZoneBGM, const FRetrieveMusicTrack& ZoneCombatBGM, bool bUseCombatMusic);

	/** 존 볼륨 이탈. 존을 모두 벗어나면 전역(전투/기본) 트랙으로 복귀. */
	void ExitMusicZone();

	/** 존 볼륨이 BeginPlay/EndPlay에서 자기 자신을 등록/해제한다(GetAllActorsOfClass 비의존). */
	void RegisterZone(ARetrieveMusicZoneVolume* Zone);
	void UnregisterZone(ARetrieveMusicZoneVolume* Zone);

	bool IsCombatActive() const { return bCombatMusicActive; }

private:
	void HandlePlayerSpotted(FGameplayTag Channel, const FEnemyPlayerSpottedPayload& Payload);
	/** 플레이어 사망 시 교전 집합을 비우고 전투 BGM을 강제 해제 */
	void HandlePlayerDied(FGameplayTag Channel, const FPlayerDiedPayload& Payload);
	/** 세션 상태(메뉴/게임플레이/결과) 전환에 맞춰 BGM 컨텍스트를 바꾼다. */
	void HandleSessionStateChanged(FGameplayTag Channel, const FRetrieveSessionStatePayload& Payload);

	/**
	 * InGame 진입 시, 플레이어가 이미 존 볼륨 안에 있으면(시작 지점 등) 오버랩 이벤트가
	 * 발생하지 않아 EnterMusicZone이 안 불린다. 폰이 possess될 때까지 기다렸다가
	 * 캡슐 오버랩을 강제 재평가해 겹친 존에 진입 이벤트를 만든다.
	 */
	void SyncPlayerZoneOverlap();

	/** 교전 집합에서 죽었거나 추적을 포기한 몬스터를 제거하고, 비면 전투 BGM을 해제한다. */
	void PollEngagement();

	/** 곡이 자연 종료되면(크로스페이드 페이드아웃 제외) LoopGapSeconds 뒤에 같은 곡을 다시 재생한다. */
	void HandleMusicFinished(UAudioComponent* Slot);

	void UpdateMusic();
	void CrossfadeTo(const FRetrieveMusicTrack& Track);
	UAudioComponent* CreateMusicSlot();

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> SlotA;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> SlotB;

	// 현재 "재생 중" 슬롯. CrossfadeTo가 매 전환마다 뒤집는다.
	bool bSlotAActive = false;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> CurrentSound;

	// 현재 곡의 볼륨 배수(자연 종료 후 리루프 재생에 재사용).
	float CurrentVolume = 1.f;
	// 곡 종료 → 재생 사이의 간격을 재는 타이머.
	FTimerHandle ReloopTimer;

	TWeakObjectPtr<APawn> LocalPlayerPawn;
	FGameplayMessageListenerHandle SpottedListenerHandle;
	FGameplayMessageListenerHandle SessionListenerHandle;
	FGameplayMessageListenerHandle PlayerDiedListenerHandle;

	// 현재 세션 상태. 메뉴/로딩이면 게임플레이(존/전투) 판정을 건너뛴다.
	ERetrieveSessionState CurrentSessionState = ERetrieveSessionState::Loading;

	// 레벨에 존재하는 존 볼륨들(자가등록). InGame 진입 후 재평가 대상.
	TArray<TWeakObjectPtr<ARetrieveMusicZoneVolume>> RegisteredZones;

	// InGame 진입 후 폰 안착까지 주기적으로 존을 재평가하는 타이머/카운터(SyncPlayerZoneOverlap).
	FTimerHandle ZoneSyncTimer;
	int32 ZoneSyncTriesLeft = 0;

	// 나를 포착해 교전 중인 몬스터 집합. 비어있지 않으면 전투 BGM.
	TSet<TWeakObjectPtr<AActor>> EngagedEnemies;
	FTimerHandle EngagementPollTimer;
	bool bCombatMusicActive = false;

	// 현재 활성 존의 트랙 세트. 존이 하나뿐이라는 가정(RetrieveWaterSuppressVolume와 동일 관례).
	// 서로 다른 설정의 존 볼륨이 겹치는 경우는 대응하지 않는다(마지막 진입이 우선).
	FRetrieveMusicTrack ZoneTrack;
	FRetrieveMusicTrack ZoneCombatTrack;
	bool bZoneUsesCombat = false;
	int32 ZoneDepth = 0;
};
