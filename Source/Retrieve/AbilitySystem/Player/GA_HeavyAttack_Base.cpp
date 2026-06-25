#include "AbilitySystem/Player/GA_HeavyAttack_Base.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Components/Player/WeaponComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "GameplayEffectTypes.h"

UGA_HeavyAttack_Base::UGA_HeavyAttack_Base()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	HitSuccessFeedbackTag = RetrieveGameplayTags::GameplayEvent_Attack_HitSuccess_Heavy;
	TargetHitFeedbackTag = RetrieveGameplayTags::GameplayEvent_Hit_Heavy;

	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_HeavyAttack);
	Tags.AddTag(RetrieveGameplayTags::Ability_Type_Attack);
	SetAssetTags(Tags);

	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Player_UsingHeavyAttack);

	// 버퍼 사용 + 공격류 우선순위. 어떤 공격에서 강공격으로 캔슬할 수 있는지는 몽타주 AllowedCancelIntents가 정한다.
	bUseCombatInputBuffer = true;
	CombatInputPriority = 10;

	// 공중/점프 중 강공격 불가
	bBlockActivationWhileAirborne = true;

	// 구르기/맨틀 등 ALS 액션 중 강공격 발동 불가. 단, 캔슬 윈도우가 허용하면 예외 — CanActivateAbility 참고.
	bBlockedByLocomotionAction = true;

	// 상태 게이트(회피/경직/다운/사망) 중 + 자기 재발동 차단
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dodging);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Knockdown);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dead);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_UsingHeavyAttack);

	CancelAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Type_Attack);

	// 시전 중 가드/대시/버스트 차단. 다른 공격 전환은 AttackCancelWindow가 게이트한다.
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Guard);
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Dash);
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Burst);
}

bool UGA_HeavyAttack_Base::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const UWeaponComponent* WeaponComp = AvatarActor ? AvatarActor->FindComponentByClass<UWeaponComponent>() : nullptr;
	const bool bIsStaff = WeaponComp && WeaponComp->GetWeaponDataRef().WeaponTypeTag == RetrieveGameplayTags::Weapon_Type_Staff;
	return bIsStaff == bActivateForStaff;
}

void UGA_HeavyAttack_Base::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ExecuteHeavyEffect(ResolveCurrentElementTag());
}

void UGA_HeavyAttack_Base::PlayHeavyMontageThenEnd()
{
	UAnimMontage* Montage = HeavyMontage.LoadSynchronous();
	if (!IsValid(Montage))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
		return;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, Montage, MontagePlayRate, NAME_None, /*bStopWhenAbilityEnds=*/true, 1.f, 0.f, /*bAllowInterruptAfterBlendOut=*/true);
	if (!MontageTask)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageFinished);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageFinished);
	MontageTask->ReadyForActivation();
}

void UGA_HeavyAttack_Base::HandleMontageFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}

void UGA_HeavyAttack_Base::ExecuteOwnerCue(const FGameplayTag& CueTag) const
{
	if (!CueTag.IsValid())
	{
		return;
	}
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		FGameplayCueParameters Params;
		Params.Instigator = GetAvatarActorFromActorInfo();
		ASC->ExecuteGameplayCue(CueTag, Params);
	}
}

void UGA_HeavyAttack_Base::AddFeedbackTagsToDamageSpec(FGameplayEffectSpec& Spec) const
{
	if (HitSuccessFeedbackTag.IsValid())
	{
		Spec.AddDynamicAssetTag(HitSuccessFeedbackTag);
	}

	if (TargetHitFeedbackTag.IsValid())
	{
		Spec.AddDynamicAssetTag(TargetHitFeedbackTag);
	}
}

void UGA_HeavyAttack_Base::ExecuteHeavyEffect(const FGameplayTag& /*ConsumedElement*/)
{
	ensureMsgf(false, TEXT("UGA_HeavyAttack_Base::ExecuteHeavyEffect 미구현: %s. 자식 클래스에서 override 필요."), *GetName());
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
}

void UGA_HeavyAttack_Base::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
