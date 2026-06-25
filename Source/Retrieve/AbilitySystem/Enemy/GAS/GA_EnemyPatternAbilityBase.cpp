#include "AbilitySystem/Enemy/GAS/GA_EnemyPatternAbilityBase.h"

#include "Animation/AnimNotifyState_AttachNiagaraToSocket.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/Enemy/EnemyCombatComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Engine/DataTable.h"
#include "NiagaraComponent.h"

UGA_EnemyPatternAbilityBase::UGA_EnemyPatternAbilityBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UGA_EnemyPatternAbilityBase::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	CleanupAttachedPatternVFX(ActorInfo);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

UEnemyCombatComponent* UGA_EnemyPatternAbilityBase::GetEnemyCombatComponent() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	return Avatar ? Avatar->FindComponentByClass<UEnemyCombatComponent>() : nullptr;
}

const FMonsterPatternRow* UGA_EnemyPatternAbilityBase::GetActivePatternRow() const
{
	const UEnemyCombatComponent* Combat = GetEnemyCombatComponent();
	if (!Combat)
	{
		return nullptr;
	}

	const UDataTable* PatternTable = Combat->GetPatternTable();
	const FName RowName = Combat->GetActivePatternRowName();
	return PatternTable && !RowName.IsNone()
		? PatternTable->FindRow<FMonsterPatternRow>(RowName, TEXT("UGA_EnemyPatternAbilityBase"))
		: nullptr;
}

float UGA_EnemyPatternAbilityBase::GetAttackSpeedMultiplier() const
{
	const UEnemyCombatComponent* Combat = GetEnemyCombatComponent();
	return Combat ? Combat->GetAttackSpeedMultiplier() : 1.f;
}

float UGA_EnemyPatternAbilityBase::GetAttackMontagePlayRate(float BasePlayRate) const
{
	const UEnemyCombatComponent* Combat = GetEnemyCombatComponent();
	return Combat ? Combat->GetAttackMontagePlayRate(BasePlayRate) : BasePlayRate;
}

void UGA_EnemyPatternAbilityBase::OnMontageCompleted()
{
}

void UGA_EnemyPatternAbilityBase::OnMontageInterrupted()
{
}

void UGA_EnemyPatternAbilityBase::CleanupAttachedPatternVFX(const FGameplayAbilityActorInfo* ActorInfo) const
{
	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	USkeletalMeshComponent* SkeletalMeshComponent = ActorInfo ? ActorInfo->SkeletalMeshComponent.Get() : nullptr;
	UAnimInstance* AnimInstance = IsValid(SkeletalMeshComponent) ? SkeletalMeshComponent->GetAnimInstance() : nullptr;
	const UAnimMontage* ActiveMontage = IsValid(AnimInstance) ? AnimInstance->GetCurrentActiveMontage() : nullptr;
	const FName AnimationTag = UAnimNotifyState_AttachNiagaraToSocket::MakeAnimationVFXComponentTag(ActiveMontage);

	if (!IsValid(AvatarActor) || AnimationTag.IsNone())
	{
		return;
	}

	TArray<UNiagaraComponent*> NiagaraComponents;
	AvatarActor->GetComponents<UNiagaraComponent>(NiagaraComponents);

	for (UNiagaraComponent* NiagaraComponent : NiagaraComponents)
	{
		if (!IsValid(NiagaraComponent))
		{
			continue;
		}

		if (!NiagaraComponent->ComponentHasTag(AnimationTag))
		{
			continue;
		}

		NiagaraComponent->Deactivate();
		NiagaraComponent->DestroyComponent();
	}
}
