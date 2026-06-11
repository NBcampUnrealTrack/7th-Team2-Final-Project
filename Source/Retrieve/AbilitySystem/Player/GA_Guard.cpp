#include "AbilitySystem/Player/GA_Guard.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "GameplayEffect.h"
#include "GameplayTags/RetrieveGameplayTags.h"

UGA_Guard::UGA_Guard()
{
	InstancingPolicy  = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ReplicationPolicy  = EGameplayAbilityReplicationPolicy::ReplicateYes;

	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_Guard);
	SetAssetTags(Tags);

	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Player_Guarding);

	// 공중/점프 중 가드 불가
	bBlockActivationWhileAirborne = true;

	// 상태 게이트(회피/경직/다운/사망) 중 가드 차단
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dodging);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Knockdown);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dead);

	// 가드 활성 중에는 공격 계열 어빌리티 차단
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Attack);
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_HeavyAttack);
}

void UGA_Guard::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	if (UAnimMontage* Montage = GuardMontage.LoadSynchronous())
	{
		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, Montage, 1.f, NAME_None, /*bStopWhenAbilityEnds=*/true);
		if (MontageTask)
		{
			MontageTask->ReadyForActivation();
		}
	}
	
	InputReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, /*bTestAlreadyReleased=*/false);
	if (InputReleaseTask)
	{
		InputReleaseTask->OnRelease.AddDynamic(this, &ThisClass::HandleInputReleased);
		InputReleaseTask->ReadyForActivation();
	}
	
	GuardBrokenTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, RetrieveGameplayTags::GameplayEvent_Guard_Broken, nullptr, /*OnlyTriggerOnce=*/true, /*OnlyMatchExact=*/true);
	if (GuardBrokenTask)
	{
		GuardBrokenTask->EventReceived.AddDynamic(this, &ThisClass::HandleGuardBroken);
		GuardBrokenTask->ReadyForActivation();
	}
	
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	const bool bParryOnCooldown = ASC && ASC->HasMatchingGameplayTag(RetrieveGameplayTags::Cooldown_Player_Parry);
	if (ParryWindowEffect && !bParryOnCooldown)
	{
		ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivationInfo, ParryWindowEffect.GetDefaultObject(), GetAbilityLevel());

		if (ParryCooldownEffect)
		{
			ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivationInfo, ParryCooldownEffect.GetDefaultObject(), GetAbilityLevel());
		}
	}

	// Parry 성공 수신 → 카운터 윈도우 부여
	ParrySuccessTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, RetrieveGameplayTags::GameplayEvent_Parry_Success, nullptr, /*OnlyTriggerOnce=*/false, /*OnlyMatchExact=*/true);
	if (ParrySuccessTask)
	{
		ParrySuccessTask->EventReceived.AddDynamic(this, &ThisClass::HandleParrySuccess);
		ParrySuccessTask->ReadyForActivation();
	}
}

void UGA_Guard::HandleParrySuccess(FGameplayEventData Payload)
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

void UGA_Guard::HandleInputReleased(float /*TimeHeld*/)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}

void UGA_Guard::HandleGuardBroken(FGameplayEventData /*Payload*/)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (IsValid(ASC) && GuardBreakStaggerEffect && HasAuthority(&GetCurrentActivationInfoRef()))
	{
		FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
		Ctx.AddSourceObject(this);

		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(GuardBreakStaggerEffect, 1.f, Ctx);
		if (Spec.IsValid())
		{
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
}

void UGA_Guard::StopRuntimeTasks()
{
	if (MontageTask)      { MontageTask->EndTask();      MontageTask = nullptr; }
	if (InputReleaseTask) { InputReleaseTask->EndTask(); InputReleaseTask = nullptr; }
	if (GuardBrokenTask)  { GuardBrokenTask->EndTask();  GuardBrokenTask = nullptr; }
	if (ParrySuccessTask) { ParrySuccessTask->EndTask(); ParrySuccessTask = nullptr; }
}

void UGA_Guard::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	StopRuntimeTasks();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
