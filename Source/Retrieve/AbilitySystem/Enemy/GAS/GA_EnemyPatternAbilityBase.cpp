#include "AbilitySystem/Enemy/GAS/GA_EnemyPatternAbilityBase.h"

#include "Components/Enemy/EnemyCombatComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Engine/DataTable.h"

UGA_EnemyPatternAbilityBase::UGA_EnemyPatternAbilityBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
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
