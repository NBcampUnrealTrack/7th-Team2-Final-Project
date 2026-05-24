// Fill out your copyright notice in the Description page of Project Settings.


#include "GA_LockOn.h"

#include "Components/CombatReactionComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"

UGA_LockOn::UGA_LockOn()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	
	AbilityTags.AddTag(RetrieveGameplayTags::Ability_Player_LockOn);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dead);
}

void UGA_LockOn::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                 const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	UE_LOG(LogTemp, Warning, TEXT("[GA_LockOn] ActivateAbility entered"));
	
	if (CommitAbility(Handle, ActorInfo, ActivationInfo) == false)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (IsValid(Avatar) == false)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	UCombatReactionComponent* ReactionComp = Avatar->FindComponentByClass<UCombatReactionComponent>();
	if (IsValid(ReactionComp) == false)
	{
		UE_LOG(LogTemp, Warning,
		TEXT("[GA_LockOn] UCombatReactionComponent not found on %s — 캐릭터에 ReactionComp 부착 확인 필요"),
		*Avatar->GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	ReactionComp->TryToggleLockOn();
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
