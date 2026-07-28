#include "AbilitySystem/Enemy/GAS/GA_Enemy_DashAttack.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/Enemy/EnemyCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "GameplayTags/RetrieveGameplayTags.h"

UGA_Enemy_DashAttack::UGA_Enemy_DashAttack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilityTags.AddTag(RetrieveGameplayTags::Ability_Enemy_SpecialAttack);
	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Enemy_SpecialAttack);

	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Dead);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Hit);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Groggy);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Attack);

	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = RetrieveGameplayTags::GameplayEvent_Enemy_SpecialAttack_DashAttack;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);
}

void UGA_Enemy_DashAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FMonsterPatternRow* PatternRow = GetActivePatternRow();
	const float FinalPlayRate = GetAttackMontagePlayRate(MontagePlayRate);

	UAnimMontage* Montage = ResolveAttackMontage(TriggerEventData, PatternRow, FinalPlayRate);
	if (!Montage || !PatternRow)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, Montage, FinalPlayRate, NAME_None, true);
	if (!MontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageInterrupted);
	MontageTask->ReadyForActivation();
}

void UGA_Enemy_DashAttack::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{

	if (AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		if (UEnemyCombatComponent* Combat = Avatar->FindComponentByClass<UEnemyCombatComponent>())
		{
			Combat->DeactivateHitbox();
		}
	}

	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

UAnimMontage* UGA_Enemy_DashAttack::ResolveAttackMontage(
	const FGameplayEventData* TriggerEventData,
	const FMonsterPatternRow* PatternRow,
	float DynamicMontagePlayRate) const
{
	if (TriggerEventData)
	{
		if (const UAnimMontage* EventMontage = Cast<UAnimMontage>(TriggerEventData->OptionalObject.Get()))
		{
			return const_cast<UAnimMontage*>(EventMontage);
		}
	}

	if (!PatternRow)
	{
		return nullptr;
	}

	if (!PatternRow->AttackMontage.IsNull())
	{
		return PatternRow->AttackMontage.LoadSynchronous();
	}

	UAnimSequenceBase* AttackSequence = PatternRow->AttackSequence.LoadSynchronous();
	AActor* Avatar = GetAvatarActorFromActorInfo();
	USkeletalMeshComponent* Mesh = Avatar ? Avatar->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!AttackSequence || !AnimInstance)
	{
		return nullptr;
	}

	return AnimInstance->PlaySlotAnimationAsDynamicMontage(
		AttackSequence,
		TEXT("DefaultSlot"),
		0.1f,
		0.15f,
		DynamicMontagePlayRate,
		1);
}


void UGA_Enemy_DashAttack::FinishAbility(bool bWasCancelled)
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}

void UGA_Enemy_DashAttack::OnMontageCompleted()
{
	FinishAbility(false);
}

void UGA_Enemy_DashAttack::OnMontageInterrupted()
{
	FinishAbility(true);
}
