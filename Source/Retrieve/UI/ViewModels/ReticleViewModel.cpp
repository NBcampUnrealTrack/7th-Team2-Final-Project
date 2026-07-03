// Fill out your copyright notice in the Description page of Project Settings.


#include "ReticleViewModel.h"

#include "AbilitySystemComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Messaging/RetrieveMessageTypes.h"

void UReticleViewModel::BindToMessage(UWorld* World)
{
	UnbindFromMessages();
	if (!World)
	{
		return;
	}
	
	ChargeListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FRetrieveBowChargePayload>(
		RetrieveGameplayTags::Channel_Bow_Charge, this, &ThisClass::HandleChargeMessage);
}

void UReticleViewModel::UnbindFromMessages()
{
	ChargeListener.Unregister();
}

void UReticleViewModel::BindToASC(UAbilitySystemComponent* InASC)
{
	UnbindFromASC();
	if (!InASC)
	{
		return;
	}

	BoundASC = InASC;
	InASC->RegisterGameplayTagEvent(RetrieveGameplayTags::State_Player_Aiming, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::HandleAimingTagChanged);

	// 바인딩 시점의 현재 조준 상태를 즉시 반영.
	const bool bNow = InASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Aiming);
	if (bNow != bIsAiming)
	{
		bIsAiming = bNow;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetIsAiming);
	}
}

void UReticleViewModel::UnbindFromASC()
{
	if (UAbilitySystemComponent* ASC = BoundASC.Get())
	{
		ASC->RegisterGameplayTagEvent(RetrieveGameplayTags::State_Player_Aiming, EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
	}
	BoundASC = nullptr;
}

void UReticleViewModel::HandleAimingTagChanged(FGameplayTag /*Tag*/, int32 NewCount)
{
	const bool bNow = NewCount > 0;
	if (bNow != bIsAiming)
	{
		bIsAiming = bNow;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetIsAiming);
	}
}

void UReticleViewModel::HandleChargeMessage(FGameplayTag Channel, const FRetrieveBowChargePayload& Payload)
{
	switch (Payload.Phase)
	{
	case ERetrieveBowChargePhase::Started:
		MaxChargeTime = Payload.MaxChargeTime;
		bIsCharging = true;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetMaxChargeTime);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetIsCharging);
		OnBowChargeStarted.Broadcast(MaxChargeTime);
		break;
	case ERetrieveBowChargePhase::Released:
		bIsCharging = false;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetIsCharging);
		OnBowChargeEnded.Broadcast(/*bReleased=*/true, Payload.ChargeRatio);
		break;
	case ERetrieveBowChargePhase::Cancelled:
		bIsCharging = false;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetIsCharging);
		OnBowChargeEnded.Broadcast(/*bReleased=*/false, 0.f);
		break;
	}
}
