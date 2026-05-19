#include "AbilitySystem/Element/GA_SetElement_Wind.h"

#include "GameplayTags/RetrieveGameplayTags.h"
#include "AbilitySystemComponent.h"

UGA_SetElement_Wind::UGA_SetElement_Wind()
{
	AbilityTags.AddTag(RetrieveGameplayTags::Ability_Player_SetElement_Wind);
}

void UGA_SetElement_Wind::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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

	ASC->AddLooseGameplayTag(RetrieveGameplayTags::Element_Wind);
	
	UE_LOG(LogTemp, Warning, TEXT("Element Mode : Wind"));

	FGameplayTag EventTag = RetrieveGameplayTags::GameplayEvent_Element_ModeChange;
	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = ActorInfo->AvatarActor.Get();
	Payload.Target = ActorInfo->AvatarActor.Get();

	ASC->HandleGameplayEvent(EventTag, &Payload);

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
