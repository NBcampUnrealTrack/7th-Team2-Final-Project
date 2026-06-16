#include "AbilitySystem/Enemy/GAS/GA_EnemyPatternSequenceAttack.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/Enemy/EnemyCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "TimerManager.h"

UGA_EnemyPatternSequenceAttack::UGA_EnemyPatternSequenceAttack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilityTags.AddTag(RetrieveGameplayTags::Ability_Enemy_Attack);
	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Enemy_Attack);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Dead);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Hit);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Groggy);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_SpecialAttack);

	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = RetrieveGameplayTags::GameplayEvent_Enemy_Attack;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);
}

void UGA_EnemyPatternSequenceAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
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

	const FMonsterPatternRow* PatternRow = ResolveActivePatternRow();
	UAnimMontage* Montage = ResolveAttackMontage(TriggerEventData, PatternRow);
	if (!Montage || !PatternRow)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ScheduleHitboxWindow(*PatternRow);

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, Montage, 1.f, NAME_None, true);
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

void UGA_EnemyPatternSequenceAttack::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitboxStartTimerHandle);
		World->GetTimerManager().ClearTimer(HitboxEndTimerHandle);
		World->GetTimerManager().ClearTimer(FinishTimerHandle);
	}

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

const FMonsterPatternRow* UGA_EnemyPatternSequenceAttack::ResolveActivePatternRow() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const UEnemyCombatComponent* Combat = Avatar ? Avatar->FindComponentByClass<UEnemyCombatComponent>() : nullptr;
	if (!Combat)
	{
		return nullptr;
	}

	const UDataTable* PatternTable = Combat->GetPatternTable();
	const FName RowName = Combat->GetActivePatternRowName();
	return PatternTable && !RowName.IsNone()
		? PatternTable->FindRow<FMonsterPatternRow>(RowName, TEXT("UGA_EnemyPatternSequenceAttack"))
		: nullptr;
}

UAnimMontage* UGA_EnemyPatternSequenceAttack::ResolveAttackMontage(
	const FGameplayEventData* TriggerEventData,
	const FMonsterPatternRow* PatternRow) const
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
		1.f,
		1);
}

void UGA_EnemyPatternSequenceAttack::ScheduleHitboxWindow(const FMonsterPatternRow& PatternRow)
{
	UWorld* World = GetWorld();
	AActor* Avatar = GetAvatarActorFromActorInfo();
	UEnemyCombatComponent* Combat = Avatar ? Avatar->FindComponentByClass<UEnemyCombatComponent>() : nullptr;
	if (!World || !Combat || PatternRow.HitboxWindowDuration <= 0.f)
	{
		return;
	}

	const float StartTime = FMath::Max(0.f, PatternRow.HitboxWindowStartTime);
	const float EndTime = StartTime + PatternRow.HitboxWindowDuration;

	World->GetTimerManager().SetTimer(
		HitboxStartTimerHandle,
		FTimerDelegate::CreateWeakLambda(Combat, [Combat]()
		{
			Combat->ActivateHitbox();
		}),
		StartTime,
		false);

	World->GetTimerManager().SetTimer(
		HitboxEndTimerHandle,
		FTimerDelegate::CreateWeakLambda(Combat, [Combat]()
		{
			Combat->DeactivateHitbox();
		}),
		EndTime,
		false);
}

void UGA_EnemyPatternSequenceAttack::FinishAbility()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UGA_EnemyPatternSequenceAttack::OnMontageCompleted()
{
	FinishAbility();
}

void UGA_EnemyPatternSequenceAttack::OnMontageInterrupted()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}
