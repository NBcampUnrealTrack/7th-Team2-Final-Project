// Fill out your copyright notice in the Description page of Project Settings.


#include "WeaponGuardAttackDefinition.h"

const FWeaponGuardAttackData* UWeaponGuardAttackDefinition::ResolveGuardAttackVariant(
	const FGameplayTag& ElementTag) const
{
	for (const FWeaponGuardAttackData& Variant : Variants)
	{
		if (Variant.ElementTag == ElementTag && !Variant.Montage.IsNull())
		{
			return &Variant;
		}
	}
	
	return !Default.Montage.IsNull() ? &Default : nullptr;
}
