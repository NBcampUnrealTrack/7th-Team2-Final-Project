#include "Audio/RetrieveMusicSubsystem.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/AudioComponent.h"
#include "Core/RetrieveGameState.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Messaging/GameplayMessages/RetrieveGameplayMessageTypes.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "Settings/RetrieveSettingsConfig.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "TimerManager.h"
#include "World/RetrieveMusicZoneVolume.h"

namespace
{
	constexpr float EngagementPollInterval = 0.5f;
}

void URetrieveMusicSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	UGameplayMessageSubsystem& Msg = UGameplayMessageSubsystem::Get(&InWorld);
	SpottedListenerHandle = Msg.RegisterListener(
		RetrieveGameplayTags::Channel_Enemy_PlayerSpotted,
		this, &URetrieveMusicSubsystem::HandlePlayerSpotted);
	SessionListenerHandle = Msg.RegisterListener(
		RetrieveGameplayTags::Channel_Session_StateChanged,
		this, &URetrieveMusicSubsystem::HandleSessionStateChanged);

	// 전환 메시지를 놓쳐도 안전하도록 현재 세션 상태를 직접 읽어 초기 BGM을 결정한다.
	if (const ARetrieveGameState* GS = InWorld.GetGameState<ARetrieveGameState>())
	{
		CurrentSessionState = GS->GetSessionState();
	}

	UpdateMusic();

	// 부팅 시 이미 InGame(메인 메뉴 스킵 등)이면 존 오버랩을 초기 동기화한다.
	if (CurrentSessionState == ERetrieveSessionState::InGame)
	{
		ZoneSyncTriesLeft = 10; // 0.3초 × 10 ≈ 3초간 재평가
		SyncPlayerZoneOverlap();
	}
}

void URetrieveMusicSubsystem::Deinitialize()
{
	if (SpottedListenerHandle.IsValid())
	{
		SpottedListenerHandle.Unregister();
	}
	if (SessionListenerHandle.IsValid())
	{
		SessionListenerHandle.Unregister();
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EngagementPollTimer);
		World->GetTimerManager().ClearTimer(ReloopTimer);
		World->GetTimerManager().ClearTimer(ZoneSyncTimer);
	}

	if (SlotA) { SlotA->Stop(); }
	if (SlotB) { SlotB->Stop(); }

	Super::Deinitialize();
}

void URetrieveMusicSubsystem::RegisterPlayer(APawn* PlayerPawn)
{
	LocalPlayerPawn = PlayerPawn;
}

void URetrieveMusicSubsystem::RegisterZone(ARetrieveMusicZoneVolume* Zone)
{
	if (Zone)
	{
		RegisteredZones.AddUnique(Zone);
	}
}

void URetrieveMusicSubsystem::UnregisterZone(ARetrieveMusicZoneVolume* Zone)
{
	RegisteredZones.Remove(Zone);
}

void URetrieveMusicSubsystem::EnterMusicZone(const FRetrieveMusicTrack& ZoneBGM, const FRetrieveMusicTrack& ZoneCombatBGM, bool bUseCombatMusic)
{
	++ZoneDepth;
	ZoneTrack = ZoneBGM;
	ZoneCombatTrack = ZoneCombatBGM;
	bZoneUsesCombat = bUseCombatMusic;
	UpdateMusic();
}

void URetrieveMusicSubsystem::ExitMusicZone()
{
	ZoneDepth = FMath::Max(0, ZoneDepth - 1);
	if (ZoneDepth == 0)
	{
		ZoneTrack = FRetrieveMusicTrack();
		ZoneCombatTrack = FRetrieveMusicTrack();
		bZoneUsesCombat = false;
	}
	UpdateMusic();
}

void URetrieveMusicSubsystem::HandlePlayerSpotted(FGameplayTag Channel, const FEnemyPlayerSpottedPayload& Payload)
{
	// 진입은 오직 "나를 포착한" 신호일 때만. 포착한 몬스터를 교전 집합에 넣는다
	// (공격/무기 장착으로는 전투 BGM에 들어가지 않는다).
	if (!LocalPlayerPawn.IsValid() || Payload.SpottedActor.Get() != LocalPlayerPawn.Get())
	{
		return;
	}

	AActor* Enemy = Payload.InstigatorEnemy.Get();
	if (!Enemy)
	{
		return;
	}

	EngagedEnemies.Add(Enemy);

	if (!bCombatMusicActive)
	{
		bCombatMusicActive = true;
		UpdateMusic();
	}

	// 교전이 시작되면 폴링으로 "추적 중단/사망" 몬스터를 걷어내며 집합이 빌 때를 감지한다.
	if (UWorld* World = GetWorld())
	{
		if (!World->GetTimerManager().IsTimerActive(EngagementPollTimer))
		{
			World->GetTimerManager().SetTimer(EngagementPollTimer, this,
				&URetrieveMusicSubsystem::PollEngagement, EngagementPollInterval, /*bLoop=*/true);
		}
	}
}

void URetrieveMusicSubsystem::HandleSessionStateChanged(FGameplayTag Channel, const FRetrieveSessionStatePayload& Payload)
{
	CurrentSessionState = Payload.NewState;
	UpdateMusic();

	// 메뉴에서 게임을 시작하면 플레이어가 이미 시작 지점 존 안에 있는데, 그 상태로 possess/카메라
	// 전환이 일어나 진입 오버랩 이벤트가 누락된다. InGame 진입 시 폰을 확보해 오버랩을 재평가한다.
	if (Payload.NewState == ERetrieveSessionState::InGame)
	{
		ZoneSyncTriesLeft = 10; // 0.3초 × 10 ≈ 3초간 재평가(폰 안착 대기)
		SyncPlayerZoneOverlap();
	}
}

void URetrieveMusicSubsystem::SyncPlayerZoneOverlap()
{
	UWorld* World = GetWorld();
	if (!World || CurrentSessionState != ERetrieveSessionState::InGame)
	{
		return;
	}

	// 폰이 possess될 때까지 대기(카메라/세션 전환 직후엔 아직 없을 수 있음).
	APawn* Pawn = LocalPlayerPawn.Get();
	if (!Pawn)
	{
		if (const APlayerController* PC = World->GetFirstPlayerController())
		{
			Pawn = PC->GetPawn();
		}
	}

	// 등록된 존들이 현재 겹침을 직접 재평가한다(BeginOverlap 이벤트를 놓쳤어도 보정).
	if (Pawn)
	{
		for (const TWeakObjectPtr<ARetrieveMusicZoneVolume>& ZoneWeak : RegisteredZones)
		{
			if (ARetrieveMusicZoneVolume* Zone = ZoneWeak.Get())
			{
				Zone->RefreshPlayerOverlap();
			}
		}
	}

	// possess/카메라/텔레포트가 끝나고 플레이어가 최종 위치에 안착할 때까지 잠시 반복 재평가한다.
	if (--ZoneSyncTriesLeft > 0)
	{
		World->GetTimerManager().SetTimer(ZoneSyncTimer,
			FTimerDelegate::CreateWeakLambda(this, [this]() { SyncPlayerZoneOverlap(); }),
			0.3f, /*bLoop=*/false);
	}
}

void URetrieveMusicSubsystem::PollEngagement()
{
	for (auto It = EngagedEnemies.CreateIterator(); It; ++It)
	{
		AActor* Enemy = It->Get();
		bool bStillEngaged = false;

		if (Enemy)
		{
			if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Enemy))
			{
				// 죽음(Dead)/비활성(Idle)/추적 포기 후 복귀(Return)면 교전 종료로 본다.
				// 그 외(추적·공격·특수공격·피격·경직·그로기 등)는 모두 "교전 중"으로 유지한다.
				// Chase는 '추적 이동' 중에만 켜지므로(근접 공격·피격 중엔 꺼짐) Chase 단독 판정은 오판을 부른다.
				bStillEngaged =
					!ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Enemy_Dead) &&
					!ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Enemy_Idle) &&
					!ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Enemy_Return);
			}
		}

		if (!bStillEngaged)
		{
			It.RemoveCurrent();
		}
	}

	if (EngagedEnemies.Num() == 0)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(EngagementPollTimer);
		}
		if (bCombatMusicActive)
		{
			bCombatMusicActive = false;
			UpdateMusic();
		}
	}
}

void URetrieveMusicSubsystem::UpdateMusic()
{
	const URetrieveMusicSettings* Settings = GetDefault<URetrieveMusicSettings>();
	if (!Settings)
	{
		return;
	}

	// 메뉴/결과/로딩은 게임플레이(존/전투) 상태와 무관하게 결정된다.
	if (CurrentSessionState == ERetrieveSessionState::MainMenu)
	{
		CrossfadeTo(Settings->MenuBGM);
		return;
	}
	if (CurrentSessionState == ERetrieveSessionState::Result)
	{
		CrossfadeTo(Settings->ResultBGM);
		return;
	}
	if (CurrentSessionState == ERetrieveSessionState::Loading)
	{
		// 로딩 중엔 재생 보류(곧 MainMenu/InGame으로 전환).
		return;
	}

	// InGame / Result: 지역(존) > 전투 > 기본.
	if (ZoneDepth > 0)
	{
		// 존 안: 이 존이 전투곡을 쓰고, 전투 중이며, 전투 트랙이 지정돼 있으면 전투곡. 아니면 평상시 존곡.
		const bool bUseZoneCombat = bZoneUsesCombat && bCombatMusicActive && !ZoneCombatTrack.Sound.IsNull();
		CrossfadeTo(bUseZoneCombat ? ZoneCombatTrack : ZoneTrack);
		return;
	}

	// 존 밖: 전역 전투/기본 트랙.
	CrossfadeTo(bCombatMusicActive ? Settings->CombatBGM : Settings->DefaultBGM);
}

UAudioComponent* URetrieveMusicSubsystem::CreateMusicSlot()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UAudioComponent* Slot = NewObject<UAudioComponent>(this);
	Slot->bAutoActivate = false;
	Slot->bAutoDestroy = false;
	Slot->bAllowSpatialization = false;
	Slot->bIsUISound = true; // 게임 일시정지(옵션 메뉴 등)에도 BGM 유지.

	if (const URetrieveSettingsConfig* Cfg = GetDefault<URetrieveSettingsConfig>())
	{
		// BGM을 Music SoundClass에 묶어 옵션의 '음악 볼륨' 슬라이더(SoundMix override)가 그대로 적용되게 한다.
		Slot->SoundClassOverride = Cfg->MusicSoundClass.LoadSynchronous();
	}

	// 곡이 자연 종료되면 간격을 두고 다시 재생하기 위해 구독(간격 루프).
	Slot->OnAudioFinishedNative.AddUObject(this, &URetrieveMusicSubsystem::HandleMusicFinished);

	Slot->RegisterComponentWithWorld(World);
	return Slot;
}

void URetrieveMusicSubsystem::CrossfadeTo(const FRetrieveMusicTrack& Track)
{
	USoundBase* NewSound = Track.Sound.LoadSynchronous();
	if (NewSound == CurrentSound)
	{
		// 같은 곡이면 유지(트랙별 볼륨은 고정 전제라 볼륨만 바뀌는 경우는 다루지 않는다).
		return;
	}
	CurrentSound = NewSound;
	CurrentVolume = FMath::Max(0.f, Track.Volume);

	// 곡이 바뀌었으니 이전 곡의 리루프 예약을 취소한다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloopTimer);
	}

	if (!SlotA) { SlotA = CreateMusicSlot(); }
	if (!SlotB) { SlotB = CreateMusicSlot(); }
	if (!SlotA || !SlotB)
	{
		return;
	}

	UAudioComponent* Outgoing = bSlotAActive ? SlotA : SlotB;
	UAudioComponent* Incoming = bSlotAActive ? SlotB : SlotA;

	const URetrieveMusicSettings* Settings = GetDefault<URetrieveMusicSettings>();
	const float FadeDuration = Settings ? Settings->CrossfadeDuration : 1.5f;

	Outgoing->FadeOut(FadeDuration, 0.f);

	if (NewSound)
	{
		Incoming->SetSound(NewSound);
		Incoming->FadeIn(FadeDuration, FMath::Max(0.f, Track.Volume));
		bSlotAActive = !bSlotAActive;
	}
}

void URetrieveMusicSubsystem::HandleMusicFinished(UAudioComponent* Slot)
{
	// 크로스페이드로 페이드아웃된 이전 곡, 또는 이미 다른 곡으로 넘어간 경우는 리루프 대상이 아니다.
	UAudioComponent* Active = bSlotAActive ? SlotA : SlotB;
	if (!Slot || Slot != Active || !CurrentSound || Slot->GetSound() != CurrentSound)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const URetrieveMusicSettings* Settings = GetDefault<URetrieveMusicSettings>();
	const float Gap = Settings ? FMath::Max(0.f, Settings->LoopGapSeconds) : 3.f;

	// 간격 뒤에 같은 곡을 다시 재생. 그 사이 곡이 바뀌면(콜백 시점 재확인) 재생하지 않는다.
	World->GetTimerManager().SetTimer(ReloopTimer, FTimerDelegate::CreateWeakLambda(this, [this, Slot]()
	{
		if (Slot && CurrentSound && Slot->GetSound() == CurrentSound)
		{
			Slot->FadeIn(0.5f, CurrentVolume); // 재시작을 짧게 페이드인.
		}
	}), FMath::Max(0.05f, Gap), /*bLoop=*/false);
}
