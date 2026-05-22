#include "AbilitySystem/Player/GA_Absorb.h"

#include "GameplayTags/RetrieveGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Components/ElementGaugeComponent.h"

UGA_Absorb::UGA_Absorb()
{
	AbilityTags.AddTag(RetrieveGameplayTags::Ability_Player_Absorb);
}

void UGA_Absorb::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    AActor* Avatar = ActorInfo->AvatarActor.Get();
    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
    if (!IsValid(Avatar) || !IsValid(ASC))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Character에 부착된 ElementGaugeComponent 조회
    UElementGaugeComponent* Gauge = Avatar->FindComponentByClass<UElementGaugeComponent>();
    if (!IsValid(Gauge))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 원소슬롯 하나 소비
    const FGameplayTag ConsumedElement = Gauge->ConsumeOldestSlot();

    // 빈 태그 또는 Element.None
    if (!ConsumedElement.IsValid() || ConsumedElement.MatchesTagExact(RetrieveGameplayTags::Element_None))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // ElementTag GE 매핑 조회
    const TSubclassOf<UGameplayEffect>* EffectClassPtr = ElementToAbsorbEffect.Find(ConsumedElement);
    if (!EffectClassPtr || !*EffectClassPtr)
    {
        // 매핑이 없으면 BP 데이터 누락
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // GE 적용
    FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
    Context.AddSourceObject(this);

    const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(*EffectClassPtr, GetAbilityLevel(), Context);

    if (SpecHandle.IsValid())
    {
        ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
