
#include "WeaponAttackDefinition.h"

#include "RetrieveDataTableTypes.h"

const FAttackComboVariant* UWeaponAttackDefinition::ResolveComboVariant(const FGameplayTag& ElementTag) const
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

const FWeaponSprintAttack* UWeaponAttackDefinition::ResolveSprintVariant(const FGameplayTag& ElementTag) const
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

const FWeaponSprintAttack* UWeaponAttackDefinition::ResolveBashVariant(const FGameplayTag& ElementTag) const
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

const FWeaponJumpAttack* UWeaponAttackDefinition::ResolveJumpVariant(const FGameplayTag& ElementTag) const
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

const FWeaponAbsorbCast* UWeaponAttackDefinition::ResolveAbsorbVariant(const FGameplayTag& ElementTag) const
{
	for (const FWeaponAbsorbCast& Variant : AbsorbVariants)
	{
		if (Variant.ElementTag == ElementTag && !Variant.Montage.IsNull())
		{
			return &Variant;
		}
	}

	return !AbsorbDefault.Montage.IsNull() ? &AbsorbDefault : nullptr;
}

const FParryCounterData* UWeaponAttackDefinition::ResolveParryVariant(const FGameplayTag& ElementTag) const
{
	for (const FParryCounterData& Variant : Parry.CounterVariants)
	{
		if (Variant.ElementTag == ElementTag && !Variant.CounterMontage.IsNull())
		{
			return &Variant;
		}
	}
	return !Parry.CounterDefault.CounterMontage.IsNull() ? &Parry.CounterDefault : nullptr;
}

const FWeaponHeavyAttack* UWeaponAttackDefinition::ResolveHeavyVariant(const FGameplayTag& ElementTag) const
{
	for (const FWeaponHeavyAttack& Variant : HeavyVariants)
	{
		if (Variant.ElementTag == ElementTag && !Variant.Montage.IsNull())
		{
			return &Variant;
		}
	}
	return nullptr;
}
