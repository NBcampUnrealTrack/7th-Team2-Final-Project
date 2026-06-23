
#include "AttackComboDefinition.h"

#include "RetrieveDataTableTypes.h"

const FAttackComboVariant* UAttackComboDefinition::ResolveComboVariant(const FGameplayTag& ElementTag) const
{
	for (const FAttackComboVariant& Variant : ElementVariants)
	{
		if (Variant.ElementTag == ElementTag && !Variant.ComboSteps.IsEmpty() && !Variant.Montage.IsNull())
		{
			return &Variant;
		}
	}

	return (!DefaultVariant.ComboSteps.IsEmpty() && !DefaultVariant.Montage.IsNull()) ? &DefaultVariant : nullptr;
}

const FWeaponSprintAttack* UAttackComboDefinition::ResolveSprintVariant(const FGameplayTag& ElementTag) const
{
	for (const FWeaponSprintAttack& Variant : SprintVariants)
	{
		if (Variant.ElementTag == ElementTag && !Variant.Montage.IsNull())
		{
			return &Variant;
		}
	}
	
	return !SprintDefault.Montage.IsNull() ? &SprintDefault : nullptr;
}

const FWeaponSprintAttack* UAttackComboDefinition::ResolveBashVariant(const FGameplayTag& ElementTag) const
{
	for (const FWeaponSprintAttack& Variant : BashVariants)
	{
		if (Variant.ElementTag == ElementTag && !Variant.Montage.IsNull())
		{
			return &Variant;
		}
	}

	return !BashDefault.Montage.IsNull() ? &BashDefault : nullptr;
}

const FWeaponJumpAttack* UAttackComboDefinition::ResolveJumpVariant(const FGameplayTag& ElementTag) const
{
	
	for (const FWeaponJumpAttack& Variant : JumpVariants)
	{
		if (Variant.ElementTag == ElementTag && !Variant.Montage.IsNull())
		{
			return &Variant;
		}
	}
	return !JumpDefault.Montage.IsNull() ? &JumpDefault : nullptr;
}

const FParryCounterData* UAttackComboDefinition::ResolveParryVariant(const FGameplayTag& ElementTag) const
{
	for (const FParryCounterData& Variant : ParryVariants)
	{
		if (Variant.ElementTag == ElementTag && !Variant.CounterMontage.IsNull())
		{
			return &Variant;
		}
	}
	return !ParryDefault.CounterMontage.IsNull() ? &ParryDefault : nullptr;
}
