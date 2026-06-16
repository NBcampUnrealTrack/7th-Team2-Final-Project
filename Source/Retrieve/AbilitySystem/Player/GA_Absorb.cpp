#include "AbilitySystem/Player/GA_Absorb.h"

#include "AbilitySystemComponent.h"
#include "Components/Element/ElementGaugeComponent.h"
#include "GameplayEffect.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "UI/HUD/RetrieveBuffUIBroadcastComponent.h"
#include "UObject/SoftObjectPath.h"

namespace
{
FGameplayTag ResolveAbsorbBuffUITag(FGameplayTag ElementTag, const TMap<FGameplayTag, FGameplayTag>& OverrideMap)
{
	if (const FGameplayTag* BuffUITag = OverrideMap.Find(ElementTag))
	{
		return *BuffUITag;
	}

	if (ElementTag.MatchesTagExact(RetrieveGameplayTags::Element_Fire))
	{
		return RetrieveGameplayTags::UI_Buff_Absorb_Fire;
	}
	if (ElementTag.MatchesTagExact(RetrieveGameplayTags::Element_Water))
	{
		return RetrieveGameplayTags::UI_Buff_Absorb_Water;
	}
	if (ElementTag.MatchesTagExact(RetrieveGameplayTags::Element_Wind))
	{
		return RetrieveGameplayTags::UI_Buff_Absorb_Wind;
	}

	return FGameplayTag();
}

TSubclassOf<UGameplayEffect> LoadDefaultAbsorbEffect(FGameplayTag ElementTag)
{
	const TCHAR* EffectPath = nullptr;
	if (ElementTag.MatchesTagExact(RetrieveGameplayTags::Element_Fire))
	{
		EffectPath = TEXT("/Game/Retrieve/AbilitySystem/Player/GE_Absorb_Fire.GE_Absorb_Fire_C");
	}
	else if (ElementTag.MatchesTagExact(RetrieveGameplayTags::Element_Water))
	{
		EffectPath = TEXT("/Game/Retrieve/AbilitySystem/Player/GE_Absorb_Water.GE_Absorb_Water_C");
	}
	else if (ElementTag.MatchesTagExact(RetrieveGameplayTags::Element_Wind))
	{
		EffectPath = TEXT("/Game/Retrieve/AbilitySystem/Player/GE_Absorb_Wind.GE_Absorb_Wind_C");
	}

	return EffectPath ? FSoftClassPath(EffectPath).TryLoadClass<UGameplayEffect>() : nullptr;
}
}

UGA_Absorb::UGA_Absorb()
{
	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_Absorb);
	SetAssetTags(Tags);
}

void UGA_Absorb::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AActor* Avatar = ActorInfo->AvatarActor.Get();
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (!IsValid(Avatar) || !IsValid(ASC))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UElementGaugeComponent* Gauge = Avatar->FindComponentByClass<UElementGaugeComponent>();
	if (!IsValid(Gauge))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FGameplayTag TargetElement = Gauge->PeekOldestSlot();
	if (!TargetElement.IsValid() || TargetElement.MatchesTagExact(RetrieveGameplayTags::Element_None))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	TSubclassOf<UGameplayEffect> EffectClass = nullptr;
	if (const TSubclassOf<UGameplayEffect>* EffectClassPtr = ElementToAbsorbEffect.Find(TargetElement))
	{
		EffectClass = *EffectClassPtr;
	}
	if (!EffectClass)
	{
		EffectClass = LoadDefaultAbsorbEffect(TargetElement);
	}
	if (!EffectClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FGameplayTag ConsumedElement = Gauge->ConsumeOldestSlot();
	if (!ConsumedElement.MatchesTagExact(TargetElement))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);

	const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(EffectClass, GetAbilityLevel(), Context);
	if (SpecHandle.IsValid())
	{
		const FActiveGameplayEffectHandle AppliedHandle =
			ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);

		const FGameplayTag BuffUITag = ResolveAbsorbBuffUITag(ConsumedElement, ElementToAbsorbBuffUITag);
		if (BuffUITag.IsValid())
		{
			if (URetrieveBuffUIBroadcastComponent* BuffUI =
				Avatar->FindComponentByClass<URetrieveBuffUIBroadcastComponent>())
			{
				const float GEDuration = SpecHandle.Data->GetDuration();
				// GE가 보고한 실제 스택 수를 UI에 그대로 전달 → StackLimitCount cap이 그대로 반영된다.
				const int32 StackCount = ASC->GetCurrentStackCount(AppliedHandle);
				BuffUI->BroadcastBuffManual(BuffUITag, GEDuration > 0.f ? GEDuration : 0.f, EffectClass, StackCount);
			}
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
