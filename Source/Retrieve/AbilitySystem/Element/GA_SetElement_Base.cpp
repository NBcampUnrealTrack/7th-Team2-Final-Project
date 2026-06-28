#include "AbilitySystem/Element/GA_SetElement_Base.h"

#include "GameplayTags/RetrieveGameplayTags.h"
#include "AbilitySystemComponent.h"

UGA_SetElement_Base::UGA_SetElement_Base()
{
	// 원소 전환은 버퍼를 쓰고 우선순위가 가장 높다(공격 도중 캔슬 윈도우에서 평타·다른 공격보다 먼저 소비).
	bUseCombatInputBuffer = true;
	CombatInputPriority = 20;
}

void UGA_SetElement_Base::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!IsValid(ASC) || !ElementTag.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ASC->SetLooseGameplayTagCount(RetrieveGameplayTags::Element_Fire, 0);
	ASC->SetLooseGameplayTagCount(RetrieveGameplayTags::Element_Water, 0);
	ASC->SetLooseGameplayTagCount(RetrieveGameplayTags::Element_Wind, 0);
	ASC->SetLooseGameplayTagCount(RetrieveGameplayTags::Element_None, 0);

	ASC->AddLooseGameplayTag(ElementTag);

	AActor* Avatar = ActorInfo->AvatarActor.Get();

	FGameplayEventData Payload;
	Payload.EventTag = RetrieveGameplayTags::GameplayEvent_Element_ModeChange;
	Payload.Instigator = Avatar;
	Payload.Target = Avatar;
	Payload.InstigatorTags.AddTag(ElementTag);

	ASC->HandleGameplayEvent(Payload.EventTag, &Payload);

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
