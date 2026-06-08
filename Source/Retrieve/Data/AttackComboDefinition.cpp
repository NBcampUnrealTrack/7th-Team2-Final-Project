
#include "AttackComboDefinition.h"

#include "RetrieveDataTableTypes.h"

const FAttackComboVariant* UAttackComboDefinition::ResolveVariant(const FGameplayTag& ElementTag) const
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
