#include "AbilitySystem/Effects/RetrieveStaminaCostEffect.h"

#include "AbilitySystem/Attributes/CombatAttributeSet.h"
#include "GameplayTags/RetrieveGameplayTags.h"

URetrieveStaminaCostEffect::URetrieveStaminaCostEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = RetrieveGameplayTags::Data_Cost_Stamina;

	FGameplayModifierInfo ModInfo;
	ModInfo.Attribute = UCombatAttributeSet::GetStaminaAttribute();
	ModInfo.ModifierOp = EGameplayModOp::Additive;
	ModInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

	Modifiers.Add(ModInfo);
}
