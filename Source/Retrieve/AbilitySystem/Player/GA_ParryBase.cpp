#include "AbilitySystem/Player/GA_ParryBase.h"

#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Components/Player/WeaponComponent.h"
#include "Data/WeaponAttackDefinition.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

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
	
	if (StaminaRestoreEffect)
	{
		ApplyGameplayEffectToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, StaminaRestoreEffect.GetDefaultObject(), GetAbilityLevel());
	}

	// 성공 피드백: 큐(VFX/SFX/카메라셰이크) + 방어자 리액션 몽타주 + 히트스톱
	ExecuteParrySuccessCue();
	PlayParrySuccessMontage();
	TriggerHitStop();

	// 적 반응 확정: 비보스=ParryStaggerEffect, 보스=BossParryStaggerEffect(약화)
	ApplyParryStagger(LastParriedAttacker.Get());
}

void UGA_ParryBase::ExecuteParrySuccessCue() const
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		FGameplayCueParameters Params;
		Params.Instigator = GetAvatarActorFromActorInfo();
		Params.EffectCauser = LastParriedAttacker.Get();
		ASC->ExecuteGameplayCue(RetrieveGameplayTags::GameplayCue_Parry_Success, Params);
	}
}

void UGA_ParryBase::PlayParrySuccessMontage()
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const UWeaponComponent* Weapon = Avatar ? Avatar->FindComponentByClass<UWeaponComponent>() : nullptr;
	if (!Weapon)
	{
		return;
	}

	const UWeaponAttackDefinition* AttackDef = Weapon->GetWeaponDataRef().AttackComboDefinition.LoadSynchronous();
	if (!AttackDef)
	{
		return;
	}

	UAnimMontage* Montage = AttackDef->ParrySuccessMontage.LoadSynchronous();
	if (!IsValid(Montage))
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->PlayMontage(this, GetCurrentActivationInfoRef(), Montage, AttackDef->ParrySuccessMontagePlayRate);
	}
}

void UGA_ParryBase::ApplyParryStagger(AActor* Attacker)
{
	if (!HasAuthority(&GetCurrentActivationInfoRef()) || !IsValid(Attacker))
	{
		return;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Attacker);
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!TargetASC || !SourceASC)
	{
		return;
	}

	const TSubclassOf<UGameplayEffect> StaggerGE = SelectEffectByTargetType(TargetASC, ParryStaggerEffect, BossParryStaggerEffect);
	const FGameplayEffectSpecHandle Spec = MakeSourcedSpec(StaggerGE, GetAbilityLevel());
	if (Spec.IsValid())
	{
		SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	}
}

void UGA_ParryBase::TriggerHitStop()
{
	UWorld* World = GetWorld();
	if (!World || HitStopTimeDilation >= 1.f || HitStopDuration <= 0.f)
	{
		return;
	}
	
	if (World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	UGameplayStatics::SetGlobalTimeDilation(World, HitStopTimeDilation);
	
	const float GameSeconds = HitStopDuration * HitStopTimeDilation;
	World->GetTimerManager().SetTimer(HitStopTimerHandle, this, &UGA_ParryBase::RestoreTimeDilation, GameSeconds, false);
}

void UGA_ParryBase::RestoreTimeDilation()
{
	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::SetGlobalTimeDilation(World, 1.f);
	}
}

void UGA_ParryBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	StopParrySuccessTask();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
