#include "AbilitySystem/Attributes/CombatAttributeSet.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UCombatAttributeSet::UCombatAttributeSet()
{
	InitHealth(100.f);
	InitMaxHealth(100.f);
}

void UCombatAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UCombatAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCombatAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
}

void UCombatAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
}

void UCombatAttributeSet::PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float DamageDone = GetIncomingDamage();
		SetIncomingDamage(0.f);

		if (DamageDone > 0.f)
		{
			const float NewHealth = FMath::Clamp(GetHealth() - DamageDone, 0.f, GetMaxHealth());
			SetHealth(NewHealth);
			BroadcastHitEvent(Data, DamageDone);
		}
	}
	else if (Data.EvaluatedData.Attribute == GetIncomingHealingAttribute())
	{
		const float HealingDone = GetIncomingHealing();
		SetIncomingHealing(0.f);

		if (HealingDone > 0.f)
		{
			const float NewHealth = FMath::Clamp(GetHealth() + HealingDone, 0.f, GetMaxHealth());
			SetHealth(NewHealth);
		}
	}
}

void UCombatAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCombatAttributeSet, Health, OldValue);
}

void UCombatAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCombatAttributeSet, MaxHealth, OldValue);
}

void UCombatAttributeSet::BroadcastHitEvent(const struct FGameplayEffectModCallbackData& Data, float DamageDone) const
{
	AActor* AttackerActor = Data.EffectSpec.GetEffectContext().GetInstigator();
	if (IsValid(AttackerActor) == false) return;
	
	AActor* TargetActor = Data.Target.AbilityActorInfo->AvatarActor.Get();
	if (IsValid(TargetActor) == false) return;
	
	// 공격자 GE에 붙여둔 태그로 강도 판정
	FGameplayTagContainer SourceTags;
	Data.EffectSpec.GetAllAssetTags(SourceTags);
	
	FGameplayTag HitEventTag ;
	for (const FGameplayTag& Tag : SourceTags)
	{
		if (Tag.MatchesTag(RetrieveGameplayTags::GameplayEvent_Hit))
		{
			HitEventTag = Tag;
			break;
		}
	}
	
	if (HitEventTag.IsValid() == false)
	{
		HitEventTag = RetrieveGameplayTags::GameplayEvent_Hit_Normal;
	}
	
	FGameplayEventData EventData;
	EventData.EventTag = HitEventTag;
	EventData.Instigator = AttackerActor;
	EventData.Target = TargetActor;
	EventData.EventMagnitude = DamageDone;
	
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(AttackerActor, HitEventTag, EventData);
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(TargetActor, HitEventTag, EventData);
	
	// 테스트 코드
	UE_LOG(LogTemp, Log, TEXT("[HitEvent] %s Damage=%.1f Attacker=%s Target=%s"),
	*HitEventTag.ToString(), DamageDone,
	AttackerActor ? *AttackerActor->GetName() : TEXT("None"),
	TargetActor   ? *TargetActor->GetName()   : TEXT("None"));
}
