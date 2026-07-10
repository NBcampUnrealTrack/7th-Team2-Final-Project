

#include "AttackFeedbackComponent.h"

#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Components/Pawn/RetrievePawnExtensionComponent.h"
#include "Core/RetrieveGameState.h"
#include "Engine/DataTable.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "UI/HUD/RetrieveDamageFloaterWidget.h"
#include "Settings/RetrieveGameUserSettings.h"

UAttackFeedbackComponent::UAttackFeedbackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAttackFeedbackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromASC();
	// 리스너 해제
	if (DamageListener.IsValid())
	{
		UWorld* World = GetWorld();
		if (IsValid(World))
		{
			UGameplayMessageSubsystem::Get(World).UnregisterListener(DamageListener);
		}
		DamageListener = FGameplayMessageListenerHandle();
	}
	// 플로터 정리(항상)
	for (URetrieveDamageFloaterWidget* Floater : AllFloaters)
	{
		if (IsValid(Floater) == false)
		{
			continue;
		}
		Floater->OnFinished.Unbind();
		Floater->RemoveFromParent();
	}

	AllFloaters.Reset();
	FloaterPool.Reset();

	Super::EndPlay(EndPlayReason);
}

void UAttackFeedbackComponent::BeginPlay()
{
	Super::BeginPlay();
	UWorld* World = GetWorld();
	if (IsValid(World) == false)
	{
		return;
	}

	DamageListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FRetrieveDamageDealtPayload>(
		RetrieveGameplayTags::Channel_Combat_DamageDealt,
		[this](FGameplayTag Channel, const FRetrieveDamageDealtPayload& Payload)
		{
			HandleDamageDealt(Channel, Payload);
		});
}

void UAttackFeedbackComponent::HandlePossessedPawnChanged(APawn* NewPawn)
{
	// 이전 폰 ASC 구독 해제
	UnbindFromASC();
	
	CurrentPawn = NewPawn;
	if (IsValid(NewPawn) == false)
	{
		return;
	}
	// ASC 준비 시점을 PawnExtension 훅으로 안전하게 잡음
	URetrievePawnExtensionComponent* PawnExt = URetrievePawnExtensionComponent::FindPawnExtensionComponent(NewPawn);
	if (IsValid(PawnExt) == false)
	{
		return;
	}
	
	PawnExt->OnAbilitySystemInitialized_RegisterAndCall(
		FSimpleMulticastDelegate::FDelegate::CreateUObject(
			this, &UAttackFeedbackComponent::BindASC));
}

void UAttackFeedbackComponent::BindASC()
{
	APawn* Pawn = CurrentPawn.Get();
	if (IsValid(Pawn) == false || IsValid(HitFeedbackTable) == false)
	{
		return;
	}
	
	URetrievePawnExtensionComponent* PawnExt = URetrievePawnExtensionComponent::FindPawnExtensionComponent(Pawn);
	URetrieveAbilitySystemComponent* ASC = PawnExt ? PawnExt->GetRetrieveAbilitySystemComponent() : nullptr;
	if (ASC == nullptr || GameplayEventHandle.IsValid())
	{
		return;
	}
	// 캐시/필터 빌드 
	FeedbackCache.Reset();
	SubscribedFilter.Reset();
	
	static const FString Context(TEXT("UAttackFeedbackComponent::BindASC"));
	TArray<FHitFeedback*> Rows;
	HitFeedbackTable->GetAllRows<FHitFeedback>(Context, Rows);
	for (const FHitFeedback* Row : Rows)
	{
		if (Row == nullptr)
		{
			continue;
		}
		for (const FGameplayTag& Tag : Row->HitEventTags)
		{
			if (Tag.MatchesTag(RetrieveGameplayTags::GameplayEvent_Attack_HitSuccess))
			{
				FeedbackCache.Add(Tag, *Row);
				SubscribedFilter.AddTag(Tag);
			}
			else if (Tag.MatchesTag(RetrieveGameplayTags::GameplayEvent_Hit))
			{
				// 피격 흔들림: GMS 조회용 캐시만
				FeedbackCache.Add(Tag, *Row);
			}
		}
	}

	if (SubscribedFilter.IsEmpty())
	{
		return;
	}

	GameplayEventHandle = ASC->AddGameplayEventTagContainerDelegate(
		SubscribedFilter,
		FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(
			this, &UAttackFeedbackComponent::HandleHitFeedback));

	BoundASC = ASC;
}

void UAttackFeedbackComponent::UnbindFromASC()
{
	if (GameplayEventHandle.IsValid() == false)
	{
		return;
	}
	
	URetrieveAbilitySystemComponent* ASC = BoundASC.Get();
	if (IsValid(ASC) == false)
	{
		GameplayEventHandle.Reset();
		SubscribedFilter.Reset();
		FeedbackCache.Reset();
		BoundASC.Reset();
		return;
	}
	
	ASC->RemoveGameplayEventTagContainerDelegate(SubscribedFilter, GameplayEventHandle);
	GameplayEventHandle.Reset();
	SubscribedFilter.Reset();
	FeedbackCache.Reset();
	BoundASC.Reset();
}

void UAttackFeedbackComponent::HandleHitFeedback(FGameplayTag EventTag, const FGameplayEventData* Payload)
{
	// 본인이 가한 공격만 처리
	if (Payload == nullptr || Payload->Instigator != CurrentPawn.Get())
	{
		return;
	}
	
	const FHitFeedback* Feedback = FeedbackCache.Find(EventTag);
	if (Feedback == nullptr)
	{
		return;
	}
	PlayCameraShake(*Feedback);
}

void UAttackFeedbackComponent::PlayCameraShake(const FHitFeedback& Feedback) const
{
	const URetrieveGameUserSettings* Settings = URetrieveGameUserSettings::Get();
	if (Settings && Settings->bReduceMotion)
	{
		return;
	}

	// 시네마틱 중 셰이크 억제 — 셰이크는 카메라 매니저 레벨이라 시네캠 뷰까지 흔든다
	if (const UWorld* World = GetWorld())
	{
		if (const ARetrieveGameState* GS = World->GetGameState<ARetrieveGameState>())
		{
			if (GS->GetCinematicState().IsActive())
			{
				return;
			}
		}
	}

	if (Feedback.CameraShake.IsNull())
	{
		return;
	}
	
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (IsValid(PC) == false)
	{
		return;
	}
	
	TSubclassOf<UCameraShakeBase> ShakeClass = Feedback.CameraShake.LoadSynchronous();
	if (IsValid(ShakeClass) == false)
	{
		return;
	}
	const float UserScale = Settings ? Settings->CameraShakeScale : 1.f;
	PC->ClientStartCameraShake(ShakeClass, Feedback.CameraShakeScale * UserScale);
}

void UAttackFeedbackComponent::HandleDamageDealt(FGameplayTag Channel, const FRetrieveDamageDealtPayload& Payload)
{
	APawn* MyPawn = CurrentPawn.Get();
	if (IsValid(MyPawn) == false )
	{
		return;
	}

	const bool bIsAttacker = Payload.Instigator == MyPawn;
	const bool bIsVictim = Payload.Target == MyPawn;
	if (bIsAttacker == false && bIsVictim == false)
	{
		return;
	}
	// 가해 = 공격 강도 태그 / 피격 = 피격 강도 태그로 흔들림 행 조회
	const FGameplayTag LookupTag = bIsAttacker ? Payload.HitEventTag : Payload.TargetEventTag;
	const FHitFeedback* Feedback = FeedbackCache.Find(LookupTag);
	if (Feedback == nullptr)
	{
		return;
	}

	if (bIsAttacker)
	{
		SpawnDamageFloater(Payload.Target, Payload.DamageAmount, *Feedback);
	}
	else
	{
		PlayCameraShake(*Feedback);
	}
}

void UAttackFeedbackComponent::SpawnDamageFloater(const AActor* Target, float DamageValue, const FHitFeedback& Feedback)
{
	if (const URetrieveGameUserSettings* Settings = URetrieveGameUserSettings::Get();
		Settings && !Settings->bShowDamageNumbers)
	{
		return;
	}

	if (IsValid(Target) == false || IsValid(DamageFloaterClass) == false)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (IsValid(PC) == false)
	{
		return;
	}

	// 머리 위 월드 위치 -> 스크린
	const FVector WorldLoc = Target->GetActorLocation() + FVector(0.f, 0.f, Feedback.FloaterWorldZOffset);
	FVector2D ScreenPos;
	if (UGameplayStatics::ProjectWorldToScreen(PC, WorldLoc, ScreenPos, false) == false)
	{
		return;
	}

	URetrieveDamageFloaterWidget* Floater = nullptr;
	if (FloaterPool.Num() > 0)
	{
		Floater = FloaterPool.Pop(EAllowShrinking::No);
	}
	else if (AllFloaters.Num() < MaxDamageFloaters)
	{
		Floater = CreateWidget<URetrieveDamageFloaterWidget>(PC, DamageFloaterClass);
		if (IsValid(Floater))
		{
			Floater->OnFinished.BindUObject(this, &UAttackFeedbackComponent::ReleaseFloater);
			AllFloaters.Add(Floater);
		}
	}

	if (IsValid(Floater) == false)
	{
		return;
	}

	if (Floater->IsInViewport() == false)
	{
		Floater->AddToViewport(FloaterZOrder);
	}
	Floater->SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
	Floater->SetPositionInViewport(ScreenPos, true);
	Floater->Activate(DamageValue, Feedback.DamageNumberScale, Feedback.DamageNumberColor);
}

void UAttackFeedbackComponent::ReleaseFloater(URetrieveDamageFloaterWidget* Floater)
{
	if (IsValid(Floater) == false)
	{
		return;
	}

	Floater->RemoveFromParent();
	FloaterPool.AddUnique(Floater);
}
