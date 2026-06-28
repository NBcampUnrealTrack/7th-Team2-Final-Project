#include "AbilitySystem/Player/GA_Guard.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Components/Player/WeaponComponent.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "TimerManager.h"

UGA_Guard::UGA_Guard()
{
	InstancingPolicy  = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ReplicationPolicy  = EGameplayAbilityReplicationPolicy::ReplicateYes;

	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_Guard);
	SetAssetTags(Tags);

	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Player_Guarding);
	ActivationPolicy = ERetrieveAbilityActivationPolicy::WhileInputActive;
	
	// 공중/회피·경직·다운·사망 중 가드 차단(플레이어 액션 공통 게이트)
	ApplyCommonActionBlocks();

	// Guard 중 일반 공격류는 막되, GuardAttack은 예외로 통과시킨다.
	// GuardAttack도 Ability.Type.Attack을 갖기 때문에 Ability.Type.Attack 전체를 막으면
	// Guard 중 Attack 입력이 GuardAttack으로 치환되어도 발동 전에 차단된다.
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Attack);
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_HeavyAttack);
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_SprintAttack);
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_JumpAttack);
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_BowShot);
}

bool UGA_Guard::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// 막기는 방패 전용 (그 외 무기는 GA_Parry)
	const AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const UWeaponComponent* WeaponComp = AvatarActor ? AvatarActor->FindComponentByClass<UWeaponComponent>() : nullptr;
	return WeaponComp &&
		WeaponComp->GetWeaponDataRef().WeaponTypeTag == RetrieveGameplayTags::Weapon_Type_SwordShield &&
		HasStamina(ActorInfo, MinimumStaminaToActivate);
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
	
	// 가드 지속 비용: 주기 소모 GE를 적용하고, 점검 타이머로 소진 시 종료
	if (HasAuthority(&ActivationInfo) && StaminaDrainEffect)
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
			IsValid(ASC) && !StaminaDrainHandle.IsValid())
		{
			FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
			Ctx.AddSourceObject(this);
			const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(StaminaDrainEffect, GetAbilityLevel(), Ctx);
			if (Spec.IsValid())
			{
				StaminaDrainHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			}
		}

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				GuardStaminaTimerHandle,
				this,
				&ThisClass::HandleGuardStaminaTick,
				FMath::Max(StaminaCostTickInterval, 0.01f),
				true);
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
		const FGameplayEffectSpecHandle Spec = MakeSourcedSpec(GuardBreakStaggerEffect, 1.f);
		if (Spec.IsValid())
		{
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
}

void UGA_Guard::HandleGuardStaminaTick()
{
	if (!HasStamina(CurrentActorInfo, KINDA_SMALL_NUMBER))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
	}
}

void UGA_Guard::StopRuntimeTasks()
{
	if (MontageTask)      { MontageTask->EndTask();      MontageTask = nullptr; }
	if (InputReleaseTask) { InputReleaseTask->EndTask(); InputReleaseTask = nullptr; }
	if (GuardBrokenTask)  { GuardBrokenTask->EndTask();  GuardBrokenTask = nullptr; }
	
	if (StaminaDrainHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->RemoveActiveGameplayEffect(StaminaDrainHandle);
		}
		StaminaDrainHandle.Invalidate();
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GuardStaminaTimerHandle);
	}
}

void UGA_Guard::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	StopRuntimeTasks();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
