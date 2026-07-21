#include "Subsystems/RetrieveCinematicSubsystem.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Blueprint/UserWidget.h"
#include "Components/Player/CounterTimeDilationComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/RetrieveGameState.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Settings/RetrieveSettingsConfig.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

namespace
{
	constexpr float CinematicAudioFadeSeconds = 0.8f;

	// 스킵 확인 창(초): 첫 스킵 키 입력 후 이 시간 안에 한 번 더 누르면 스킵 확정.
	constexpr float CinematicSkipConfirmWindowSeconds = 1.5f;
}

bool URetrieveCinematicSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool URetrieveCinematicSubsystem::PlayCinematic(ULevelSequence* Sequence, const FRetrieveCinematicPlayParams& Params)
{
	UWorld* World = GetWorld();
	if (!Sequence || !World || IsCinematicPlaying())
	{
		return false;
	}
	
	FMovieSceneSequencePlaybackSettings Settings;
	Settings.bDisableMovementInput = !Params.bBlockPlayerControl && Params.bDisableMovementInput;
	Settings.bDisableLookAtInput = !Params.bBlockPlayerControl && Params.bDisableLookInput;

	ALevelSequenceActor* OutActor = nullptr;
	ULevelSequencePlayer* Player = ULevelSequencePlayer::CreateLevelSequencePlayer(World, Sequence, Settings, OutActor);
	if (!Player)
	{
		return false;
	}

	ActivePlayer = Player;
	ActivePlayerActor = OutActor;
	ActiveParams = Params;
	bFinishHandled = false;
	LastSkipPressRealTime = -1.0;

	SetCinematicStateOnGameState(true);
	if (Params.bApplyCinematicTag)
	{
		ApplyCinematicTagToLocalPlayer(true);
	}
	if (Params.bBlockPlayerControl)
	{
		ApplyPlayerControlBlock(true);
	}
	ShowOverlayWidgets();
	ApplyAudioSuppression(true);
	
	if (const APlayerController* LocalPC = UGameplayStatics::GetPlayerController(World, 0))
	{
		if (APawn* LocalPawn = LocalPC->GetPawn())
		{
			if (UCounterTimeDilationComponent* CounterDilation = LocalPawn->FindComponentByClass<UCounterTimeDilationComponent>())
			{
				CounterDilation->CancelImmediately();
			}
		}
	}
	if (!FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(World), 1.f))
	{
		UGameplayStatics::SetGlobalTimeDilation(World, 1.f); 
	}

	Player->OnFinished.AddDynamic(this, &URetrieveCinematicSubsystem::HandleSequenceFinished);
	Player->OnStop.AddDynamic(this, &URetrieveCinematicSubsystem::HandleSequenceFinished);
	Player->Play();
	return true;
}

bool URetrieveCinematicSubsystem::PlayCinematicSoft(const TSoftObjectPtr<ULevelSequence>& Sequence, const FRetrieveCinematicPlayParams& Params)
{
	return PlayCinematic(Sequence.LoadSynchronous(), Params);
}

void URetrieveCinematicSubsystem::StopCinematic()
{
	if (ActivePlayer)
	{
		ActivePlayer->Stop();
	}
}

bool URetrieveCinematicSubsystem::NotifySkipKeyPressed()
{
	UWorld* World = GetWorld();
	if (!World || !IsActiveCinematicSkippable() || bFinishHandled)
	{
		return false;
	}

	const double Now = World->GetRealTimeSeconds();

	// 확인 창 안의 두 번째 입력 → 스킵 확정
	if (LastSkipPressRealTime >= 0.0 && (Now - LastSkipPressRealTime) <= CinematicSkipConfirmWindowSeconds)
	{
		LastSkipPressRealTime = -1.0;
		World->GetTimerManager().ClearTimer(SkipPromptResetTimer);
		SetSkipPromptVisible(false);
		StopCinematic(); // OnStop → HandleSequenceFinished가 태그/입력/오디오/HUD 정리를 일괄 수행
		return true;
	}

	// 첫 입력 → 확인 프롬프트 표시, 창이 지나면 자동으로 내림
	LastSkipPressRealTime = Now;
	SetSkipPromptVisible(true);
	World->GetTimerManager().SetTimer(SkipPromptResetTimer,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			LastSkipPressRealTime = -1.0;
			SetSkipPromptVisible(false);
		}),
		CinematicSkipConfirmWindowSeconds, false);
	return true;
}

void URetrieveCinematicSubsystem::SetSkipPromptVisible(bool bVisible)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FRetrieveCinematicStatePayload Payload;
	Payload.bActive = bVisible;
	UGameplayMessageSubsystem::Get(World).BroadcastMessage(
		RetrieveGameplayTags::Channel_Cinematic_SkipPrompt, Payload);
}

void URetrieveCinematicSubsystem::HandleSequenceFinished()
{
	
	if (bFinishHandled)
	{
		return;
	}
	bFinishHandled = true;

	// 스킵 확인 대기 상태 정리(자연 종료/중단 공통) — 프롬프트가 종료 후 화면에 남지 않도록.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SkipPromptResetTimer);
	}
	if (LastSkipPressRealTime >= 0.0)
	{
		LastSkipPressRealTime = -1.0;
		SetSkipPromptVisible(false);
	}

	if (ActiveParams.bApplyCinematicTag)
	{
		ApplyCinematicTagToLocalPlayer(false);
	}
	if (ActiveParams.bBlockPlayerControl)
	{
		ApplyPlayerControlBlock(false);
	}
	RemoveOverlayWidgets();
	RestoreLocalPawnAnimationMode();
	
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateWeakLambda(this, [this]() { RestoreLocalPawnAnimationMode(); }));
	}
	ApplyAudioSuppression(false);

	// 시퀀스가 Time Dilation 트랙(슬로모 연출)을 쓰는 도중 스킵/중단되면, 섹션의
	// When Finished=Restore State가 아니면 전역 슬로모가 잔류한다. PlayCinematic의
	// 시작 정규화와 대칭으로 종료 시에도 방어적으로 원복한다(자연 종료 시엔 no-op).
	if (UWorld* World = GetWorld();
		World && !FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(World), 1.f))
	{
		UGameplayStatics::SetGlobalTimeDilation(World, 1.f);
	}

	SetCinematicStateOnGameState(false);

	if (ActivePlayer)
	{
		ActivePlayer->OnFinished.RemoveDynamic(this, &URetrieveCinematicSubsystem::HandleSequenceFinished);
		ActivePlayer->OnStop.RemoveDynamic(this, &URetrieveCinematicSubsystem::HandleSequenceFinished);
	}
	ActivePlayer = nullptr;
	ActivePlayerActor = nullptr;

	OnCinematicFinished.Broadcast();
}

void URetrieveCinematicSubsystem::SetCinematicStateOnGameState(bool bActive)
{
	UWorld* World = GetWorld();
	ARetrieveGameState* GS = World ? World->GetGameState<ARetrieveGameState>() : nullptr;
	if (!GS)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RetrieveCinematic] GameState를 찾지 못해 시네마틱 상태(%d)를 전파하지 못함 - UI 억제/HUD 숨김이 동작하지 않습니다"), bActive);
		return;
	}

	GS->SetCinematicActive(bActive); // 서버 권한 체크는 내부에서 수행 (클라 호출 시 no-op)
	UE_LOG(LogTemp, Log, TEXT("[RetrieveCinematic] CinematicState 요청 %d -> 적용 후 %d (authority=%d)"),
		bActive ? 1 : 0, GS->GetCinematicState().IsActive() ? 1 : 0, GS->HasAuthority() ? 1 : 0);
}

void URetrieveCinematicSubsystem::ApplyPlayerControlBlock(bool bBlock)
{
	if (bBlock)
	{
		APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
		if (!PC)
		{
			return;
		}
		BlockedPlayerController = PC;
		BlockedPawn = PC->GetPawn();

		// 주의: PC->DisableInput(PC)는 PC 자신의 InputComponent만 입력 스택에서 제외한다.
		// 점프/공격/이동은 폰의 InputComponent에 바인딩되어 있고 이는 "폰의" InputEnabled 기준이므로
		// (BuildInputStack 참고) 폰도 함께 꺼야 어빌리티 입력까지 차단된다. UMG 위젯 입력은 영향 없음.
		PC->DisableInput(PC);
		if (APawn* Pawn = BlockedPawn.Get())
		{
			Pawn->DisableInput(PC);
		}
		PC->FlushPressedKeys(); // 연타 중 차단 시 눌림 상태 잔여 제거

		// 무브먼트를 정지시켜 시퀀서 트랜스폼/ALS 회전 갱신이 서로 싸우는 떨림을 제거.
		// 메인 메뉴(ApplyMainMenuCamera)와 동일한 패턴, 복원도 동일하게 SetDefaultMovementMode 사용.
		if (const ACharacter* Character = Cast<ACharacter>(BlockedPawn.Get()))
		{
			if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
			{
				MoveComp->StopMovementImmediately();
				MoveComp->DisableMovement();
			}
		}
	}
	else
	{
		if (APlayerController* PC = BlockedPlayerController.Get())
		{
			if (APawn* Pawn = BlockedPawn.Get())
			{
				Pawn->EnableInput(PC);
			}
			PC->EnableInput(PC);
			PC->FlushPressedKeys(); // 시네마틱 중 눌려 있던 키가 복원 직후 액션으로 새는 것 방지

			// 시퀀스 플레이어/기타 경로가 남겼을 수 있는 Ignore 카운터 잔류 청소 (look/move 영구 차단 방지)
			PC->ResetIgnoreInputFlags();

			// 백그라운드 상태에서 종료된 경우 커서/캡처가 꼬일 수 있으므로 게임 입력 모드를 재적용.
			// 세션이 InGame일 때만 — 다른 상태(메뉴 등)면 해당 상태 전환 로직(UpdateInputMode)이 관리한다.
			UWorld* World = GetWorld();
			const ARetrieveGameState* GS = World ? World->GetGameState<ARetrieveGameState>() : nullptr;
			if (GS && GS->GetSessionState() == ERetrieveSessionState::InGame)
			{
				FInputModeGameOnly Mode;
				PC->SetInputMode(Mode);
				PC->SetShowMouseCursor(false);
			}
		}
		if (const ACharacter* Character = Cast<ACharacter>(BlockedPawn.Get()))
		{
			if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
			{
				MoveComp->SetDefaultMovementMode();
			}
		}
		BlockedPlayerController = nullptr;
		BlockedPawn = nullptr;
	}
}

void URetrieveCinematicSubsystem::ShowOverlayWidgets()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC)
	{
		return;
	}

	for (const TSubclassOf<UUserWidget>& WidgetClass : ActiveParams.OverlayWidgetClasses)
	{
		if (!WidgetClass)
		{
			continue;
		}
		if (UUserWidget* Widget = CreateWidget<UUserWidget>(PC, WidgetClass))
		{
			// 패널(50)/퀵슬롯 휠(70)보다 위, 로딩 커버(100+)보다 아래
			Widget->AddToViewport(90);
			ActiveOverlayWidgets.Add(Widget);
		}
	}
}

void URetrieveCinematicSubsystem::RemoveOverlayWidgets()
{
	for (UUserWidget* Widget : ActiveOverlayWidgets)
	{
		if (Widget)
		{
			Widget->RemoveFromParent();
		}
	}
	ActiveOverlayWidgets.Reset();
}

void URetrieveCinematicSubsystem::RestoreLocalPawnAnimationMode()
{
	// 시퀀서 종료 후 애님 상태가 잔존하면(저FPS 백그라운드 종료 등) 로코모션이 깨진다(흐물흐물).
	// 메인 Mesh뿐 아니라 VisualMesh(리타겟 ABP — 화면에 보이는 쪽) 등 모든 스켈메시를 점검:
	// (1) CustomMode 잔존 → ABP 모드로 강제 복원, (2) 시퀀서 몽타주 잔재(슬롯 블렌드 고착) → 정지.
	//     시네마틱 중에는 입력이 차단되어 게임플레이 몽타주가 있을 수 없으므로 전체 정지는 안전하다.
	const APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	const ACharacter* Character = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	if (!Character)
	{
		return;
	}

	TInlineComponentArray<USkeletalMeshComponent*> MeshComponents(Character);
	for (USkeletalMeshComponent* Mesh : MeshComponents)
	{
		if (!Mesh || !Mesh->GetAnimClass())
		{
			continue;
		}
		if (Mesh->GetAnimationMode() != EAnimationMode::AnimationBlueprint)
		{
			Mesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		}
		else if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
		{
			AnimInstance->Montage_Stop(0.f);
		}
	}
}

void URetrieveCinematicSubsystem::ApplyAudioSuppression(bool bApply)
{
	UWorld* World = GetWorld();
	const URetrieveSettingsConfig* Cfg = GetDefault<URetrieveSettingsConfig>();
	if (!World || !Cfg)
	{
		return;
	}

	if (bApply)
	{
		if (!ActiveParams.bSuppressMusic && !ActiveParams.bSuppressAmbience)
		{
			return;
		}

		if (!AudioSuppressMix)
		{
			// 설정 볼륨 믹스(SettingsSoundMix)와 별개의 믹스를 푸시한다 — 활성 믹스들의 오버라이드는
			// 곱연산되므로 옵션 볼륨을 건드리지 않고 위에 얹었다 걷을 수 있다.
			AudioSuppressMix = NewObject<USoundMix>(this, TEXT("CinematicAudioSuppressMix"));
			AudioSuppressMix->FadeInTime = CinematicAudioFadeSeconds;
			AudioSuppressMix->FadeOutTime = CinematicAudioFadeSeconds; // Pop 시 페이드 복원
		}

		// 채널별 적용/해제: 이전 재생의 오버라이드가 믹스 객체에 남으므로, 이번에 끄지 않는 채널은 명시적으로 클리어한다.
		auto SetChannelSuppressed = [&](const TSoftObjectPtr<USoundClass>& SoftClass, bool bSuppress)
		{
			if (USoundClass* SoundClass = SoftClass.LoadSynchronous())
			{
				if (bSuppress)
				{
					UGameplayStatics::SetSoundMixClassOverride(World, AudioSuppressMix, SoundClass,
						/*Volume*/0.f, /*Pitch*/1.f, CinematicAudioFadeSeconds, /*bApplyToChildren*/true);
				}
				else
				{
					UGameplayStatics::ClearSoundMixClassOverride(World, AudioSuppressMix, SoundClass, 0.f);
				}
			}
		};
		SetChannelSuppressed(Cfg->MusicSoundClass, ActiveParams.bSuppressMusic);
		SetChannelSuppressed(Cfg->AmbienceSoundClass, ActiveParams.bSuppressAmbience);

		UGameplayStatics::PushSoundMixModifier(World, AudioSuppressMix);
		bAudioSuppressed = true;
	}
	else if (bAudioSuppressed)
	{
		UGameplayStatics::PopSoundMixModifier(World, AudioSuppressMix);
		bAudioSuppressed = false;
	}
}

void URetrieveCinematicSubsystem::ApplyCinematicTagToLocalPlayer(bool bApply)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC)
	{
		return;
	}

	// ASC는 PlayerState 소유(Lyra 패턴). 폰 -> 플레이어 스테이트 순으로 조회.
	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PC->GetPawn());
	if (!ASC)
	{
		ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PC->PlayerState);
	}
	if (!ASC)
	{
		return;
	}

	if (bApply)
	{
		ASC->AddLooseGameplayTag(RetrieveGameplayTags::State_Player_Cinematic);
	}
	else
	{
		ASC->RemoveLooseGameplayTag(RetrieveGameplayTags::State_Player_Cinematic);
	}
}
