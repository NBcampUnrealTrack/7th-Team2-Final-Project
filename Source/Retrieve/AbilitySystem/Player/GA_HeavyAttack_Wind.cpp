#include "AbilitySystem/Player/GA_HeavyAttack_Wind.h"

#include "GameFramework/Character.h"
#include "Components/RetrieveHeroComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"

UGA_HeavyAttack_Wind::UGA_HeavyAttack_Wind()
{
	ActivationRequiredTags.AddTag(RetrieveGameplayTags::Element_Wind);

	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_HeavyAttack);
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_HeavyAttack_Wind);
	SetAssetTags(Tags);
	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Action_HighMobility);
}

void UGA_HeavyAttack_Wind::ExecuteHeavyEffect(const FGameplayTag& /*ConsumedElement*/)
{
	ExecuteOwnerCue(RetrieveGameplayTags::GameplayCue_HeavyAttack_Wind);
	
	if (ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		const FVector Dir = ResolveLaunchDirection();
		if (!Dir.IsNearlyZero())
		{
			Character->LaunchCharacter(Dir * LaunchSpeed, /*bXYOverride=*/true, /*bZOverride=*/false);
		}
	}

	PlayHeavyMontageThenEnd();
}

FVector UGA_HeavyAttack_Wind::ResolveLaunchDirection() const
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor))
	{
		return FVector::ZeroVector;
	}

	if (const URetrieveHeroComponent* Hero = URetrieveHeroComponent::FindHeroComponent(AvatarActor))
	{
		FVector Cached = Hero->GetCachedMoveInputDirection();
		Cached.Z = 0.f;
		if (!Cached.IsNearlyZero())
		{
			return Cached.GetSafeNormal();
		}
	}

	FVector Forward = AvatarActor->GetActorForwardVector();
	Forward.Z = 0.f;
	return Forward.GetSafeNormal();
}
