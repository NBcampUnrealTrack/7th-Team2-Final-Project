#include "Settings/RetrieveStaminaSettings.h"

#include "GameplayTags/RetrieveGameplayTags.h"

URetrieveStaminaSettings::URetrieveStaminaSettings()
{
	// 기본 밸런스(0713, MaxStamina=100 기준). Project Settings에서 덮어쓸 수 있다.
	auto AddCost = [this](const FGameplayTag& Tag, float Activation, float Minimum, float Drain, float Restore)
	{
		FStaminaCostRow Row;
		Row.ActivationCost = Activation;
		Row.MinimumToActivate = Minimum;
		Row.DrainPerSecond = Drain;
		Row.RestoreAmount = Restore;
		StaminaCosts.Add(Tag, Row);
	};

	AddCost(RetrieveGameplayTags::Ability_Player_HeavyAttack, /*Activation=*/35.f, 0.f, 0.f, 0.f);
	AddCost(RetrieveGameplayTags::Ability_Player_Dash,        /*Activation=*/25.f, 0.f, 0.f, 0.f);
	AddCost(RetrieveGameplayTags::Ability_Player_Blink,       /*Activation=*/25.f, 0.f, 0.f, 0.f);
	AddCost(RetrieveGameplayTags::Ability_Player_Parry,       /*Activation=*/20.f, 0.f, 0.f, /*Restore=*/30.f);
	AddCost(RetrieveGameplayTags::Ability_Player_Guard,       0.f, /*Minimum=*/10.f, /*Drain=*/25.f, 0.f);
}
