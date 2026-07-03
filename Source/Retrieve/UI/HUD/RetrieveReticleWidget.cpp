#include "UI/HUD/RetrieveReticleWidget.h"

#include "AbilitySystemInterface.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "MVVMSubsystem.h"
#include "TimerManager.h"
#include "View/MVVMView.h"

#include "Player/RetrievePlayerController.h"
#include "UI/ViewModels/HUDViewModel.h"
#include "UI/ViewModels/ReticleViewModel.h"

void URetrieveReticleWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ARetrievePlayerController* PlayerController = Cast<ARetrievePlayerController>(GetOwningPlayer());
	if (!PlayerController)
	{
		return;
	}

	UHUDViewModel* HUDVM = PlayerController->GetHUDViewModel();
	if (!HUDVM)
	{
		return;
	}

	UReticleViewModel* ReticleVM = HUDVM->GetReticle();
	if (!ReticleVM)
	{
		return;
	}

	// 차징 시작/종료에 반응해 스프레드 타이머를 켜고 끈다.
	BoundViewModel = ReticleVM;
	ReticleVM->OnBowChargeStarted.AddDynamic(this, &ThisClass::HandleChargeStarted);
	ReticleVM->OnBowChargeEnded.AddDynamic(this, &ThisClass::HandleChargeEnded);

	// 위젯은 로컬이므로 로컬 플레이어 ASC를 직접 잡는다(조준 태그가 여기 있음).
	// LocalPredicted GA_BowAim이 State.Player.Aiming을 로컬 ASC에 올린다.
	if (APlayerState* PS = PlayerController->PlayerState)
	{
		if (const IAbilitySystemInterface* ASCI = Cast<IAbilitySystemInterface>(PS))
		{
			if (UAbilitySystemComponent* ASC = ASCI->GetAbilitySystemComponent())
			{
				ReticleVM->BindToASC(ASC);
			}
		}
	}

	// 차징 상태 메시지(Channel.Bow.Charge) 구독.
	ReticleVM->BindToMessage(GetWorld());

	// 자식 위젯 자기 자신의 MVVM 뷰에 "Reticle" 슬롯을 채운다.
	// (WBP_Reticle의 Viewmodels 탭에 이름 "Reticle", Creation Type=Manual 엔트리가 있어야 함)
	if (UMVVMSubsystem* MVVM = GEngine ? GEngine->GetEngineSubsystem<UMVVMSubsystem>() : nullptr)
	{
		if (UMVVMView* View = MVVM->GetViewFromUserWidget(this))
		{
			View->SetViewModel(TEXT("Reticle"), ReticleVM);
		}
	}
}

void URetrieveReticleWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargeTimerHandle);
	}

	if (IsValid(BoundViewModel))
	{
		BoundViewModel->OnBowChargeStarted.RemoveAll(this);
		BoundViewModel->OnBowChargeEnded.RemoveAll(this);
	}
	BoundViewModel = nullptr;

	Super::NativeDestruct();
}

void URetrieveReticleWidget::HandleChargeStarted(float /*MaxChargeTime*/)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 실시간 기준 경과 계산용 시작 시각(타이머 지터와 무관하게 GA 홀드시간과 일치).
	ChargeStartTime = World->GetTimeSeconds();

	// 차징 램프 구간에만 도는 루프 타이머.
	World->GetTimerManager().SetTimer(
		ChargeTimerHandle, this, &URetrieveReticleWidget::TickChargeTimer, ChargeUpdateInterval, /*bLoop=*/true);
}

void URetrieveReticleWidget::TickChargeTimer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Max = IsValid(BoundViewModel) ? BoundViewModel->GetMaxChargeTime() : 0.f;
	const float Elapsed = World->GetTimeSeconds() - ChargeStartTime;
	const float Ratio = (Max > 0.f) ? FMath::Clamp(Elapsed / Max, 0.f, 1.f) : 1.f;

	SetAimSpread(Ratio);

	// 풀차징 도달 → 마지막 값(1)을 유지한 채 타이머 해제(홀드 중 오버헤드 0).
	if (Ratio >= 1.f)
	{
		World->GetTimerManager().ClearTimer(ChargeTimerHandle);
	}
}

void URetrieveReticleWidget::HandleChargeEnded(bool /*bReleased*/, float /*ChargeRatio*/)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargeTimerHandle);
	}

	// 차징 종료 → 스프레드 원복. 발사 플래시가 필요하면 bReleased로 분기해 확장.
	SetAimSpread(0.f);
}