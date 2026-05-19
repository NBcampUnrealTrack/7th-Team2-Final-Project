#include "AbilitySystem/Element/GA_SetElement_Fire.h"

#include "GameplayTags/RetrieveGameplayTags.h"
#include "AbilitySystemComponent.h"

UGA_SetElement_Fire::UGA_SetElement_Fire()
{
	AbilityTags.AddTag(RetrieveGameplayTags::Ability_Player_SetElement_Fire);
}

void UGA_SetElement_Fire::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (!IsValid(ASC))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ASC->SetLooseGameplayTagCount(RetrieveGameplayTags::Element_Fire, 0);
	ASC->SetLooseGameplayTagCount(RetrieveGameplayTags::Element_Water, 0);
	ASC->SetLooseGameplayTagCount(RetrieveGameplayTags::Element_Wind, 0);
	ASC->SetLooseGameplayTagCount(RetrieveGameplayTags::Element_None, 0);

	ASC->AddLooseGameplayTag(RetrieveGameplayTags::Element_Fire);

	UE_LOG(LogTemp, Warning, TEXT("Element Mode : Fire"));

	FGameplayTag EventTag = RetrieveGameplayTags::GameplayEvent_Element_ModeChange;
	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = ActorInfo->AvatarActor.Get();
	Payload.Target = ActorInfo->AvatarActor.Get();

	ASC->HandleGameplayEvent(EventTag, &Payload);

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
