#include "AbilitySystem/Player/GA_ParryBase.h"

#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayTags/RetrieveGameplayTags.h"

void UGA_ParryBase::OpenParryWindow()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	const bool bParryOnCooldown = ASC && ASC->HasMatchingGameplayTag(RetrieveGameplayTags::Cooldown_Player_Parry);
	if (ParryWindowEffect && !bParryOnCooldown)
	{
		ApplyGameplayEffectToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, ParryWindowEffect.GetDefaultObject(), GetAbilityLevel());

		if (ParryCooldownEffect)
		{
			ApplyGameplayEffectToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, ParryCooldownEffect.GetDefaultObject(), GetAbilityLevel());
		}
	}
}

void UGA_ParryBase::StartListeningForParrySuccess()
{
	StopParrySuccessTask();

	ParrySuccessTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, RetrieveGameplayTags::GameplayEvent_Parry_Success, nullptr, /*OnlyTriggerOnce=*/false, /*OnlyMatchExact=*/true);
	if (ParrySuccessTask)
	{
		ParrySuccessTask->EventReceived.AddDynamic(this, &ThisClass::HandleParrySuccess);
		ParrySuccessTask->ReadyForActivation();
	}
}

void UGA_ParryBase::StopParrySuccessTask()
{
	if (ParrySuccessTask)
	{
		ParrySuccessTask->EndTask();
		ParrySuccessTask = nullptr;
	}
}

void UGA_ParryBase::HandleParrySuccess(FGameplayEventData Payload)
{
	LastParriedAttacker = const_cast<AActor*>(Cast<AActor>(Payload.OptionalObject));

	if (CounterWindowEffect)
	{
		ApplyGameplayEffectToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, CounterWindowEffect.GetDefaultObject(), GetAbilityLevel());
	}

	if (ParryStaggerEffect && HasAuthority(&GetCurrentActivationInfoRef()))
	{
		if (AActor* Attacker = LastParriedAttacker.Get())
		{
			UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Attacker);
			UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
			if (TargetASC && SourceASC && !TargetASC->HasMatchingGameplayTag(RetrieveGameplayTags::Monster_Type_Boss))
			{
				AActor* AvatarActor = GetAvatarActorFromActorInfo();
				FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
				Ctx.AddInstigator(AvatarActor, AvatarActor);
				Ctx.AddSourceObject(this);

				const FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(ParryStaggerEffect, GetAbilityLevel(), Ctx);
				if (Spec.IsValid())
				{
					SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
				}
			}
		}
	}
}

void UGA_ParryBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	StopParrySuccessTask();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
