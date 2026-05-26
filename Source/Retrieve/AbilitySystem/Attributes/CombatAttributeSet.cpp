#include "AbilitySystem/Attributes/CombatAttributeSet.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UCombatAttributeSet::UCombatAttributeSet()
{
	InitHealth(100.f);
	InitMaxHealth(100.f);
	InitAttackPower(0.f);
	InitMoveSpeed(600.f);
	InitIncomingDamageMultiplier(1.f);
}

void UCombatAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UCombatAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCombatAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCombatAttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCombatAttributeSet, MoveSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCombatAttributeSet, IncomingDamageMultiplier, COND_None, REPNOTIFY_Always);
}

void UCombatAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetAttackPowerAttribute())
	{
		NewValue = FMath::Max(0.f, NewValue);
	}
	else if (Attribute == GetMoveSpeedAttribute())
	{
		NewValue = FMath::Max(0.f, NewValue);
	}
	else if (Attribute == GetIncomingDamageMultiplierAttribute())
	{
		NewValue = FMath::Max(0.f, NewValue);
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

void UCombatAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCombatAttributeSet, AttackPower, OldValue);
}

void UCombatAttributeSet::OnRep_MoveSpeed(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCombatAttributeSet, MoveSpeed, OldValue);
}

void UCombatAttributeSet::OnRep_IncomingDamageMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCombatAttributeSet, IncomingDamageMultiplier, OldValue);
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
	
	FGameplayTag AttackerEventTag;
	for (const FGameplayTag& Tag : SourceTags)
	{
		if (Tag != RetrieveGameplayTags::GameplayEvent_Attack_HitSuccess &&
			Tag.MatchesTag(RetrieveGameplayTags::GameplayEvent_Attack_HitSuccess))
		{
			AttackerEventTag = Tag;
			break;
		}
	}
	
	if (AttackerEventTag.IsValid() == false)
	{
		AttackerEventTag = RetrieveGameplayTags::GameplayEvent_Attack_HitSuccess_Light;
	}
	
	FGameplayTag TargetEventTag;
	for (const FGameplayTag& Tag : SourceTags)
	{
		if (Tag != RetrieveGameplayTags::GameplayEvent_Hit &&
			Tag.MatchesTag(RetrieveGameplayTags::GameplayEvent_Hit))
		{
			TargetEventTag = Tag;
			break;
		}
	}
	
	if (TargetEventTag.IsValid() == false)
	{
		TargetEventTag = RetrieveGameplayTags::GameplayEvent_Hit_Normal;
	}
	
	FGameplayEventData EventData;
	EventData.Instigator = AttackerActor;
	EventData.Target = TargetActor;
	EventData.EventMagnitude = DamageDone;
	
	EventData.EventTag = AttackerEventTag;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(AttackerActor, AttackerEventTag, EventData);
	if (TargetActor != AttackerActor)
	{
		EventData.EventTag = TargetEventTag;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(TargetActor, TargetEventTag, EventData);
	}
	
	// 테스트 코드
	UE_LOG(LogTemp, Log, TEXT("[HitEvent] AttackerEvent=%s TargetEvent=%s Damage=%.1f Attacker=%s Target=%s"),
		*AttackerEventTag.ToString(),
		*TargetEventTag.ToString(),
		DamageDone,
		*GetNameSafe(AttackerActor),
		*GetNameSafe(TargetActor));
}
