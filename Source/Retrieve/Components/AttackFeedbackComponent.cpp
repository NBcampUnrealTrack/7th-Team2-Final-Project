

#include "AttackFeedbackComponent.h"

#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Components/RetrievePawnExtensionComponent.h"
#include "Engine/DataTable.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTags/RetrieveGameplayTags.h"

UAttackFeedbackComponent::UAttackFeedbackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAttackFeedbackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromASC();
	Super::EndPlay(EndPlayReason);
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
	// TODO: 대미지 숫자 플로터
}

void UAttackFeedbackComponent::PlayCameraShake(const FHitFeedback& Feedback) const
{
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
	PC->ClientStartCameraShake(ShakeClass, Feedback.CameraShakeScale);
}
