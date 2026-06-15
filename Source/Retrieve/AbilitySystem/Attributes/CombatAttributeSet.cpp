#include "AbilitySystem/Attributes/CombatAttributeSet.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "GameplayEffectExtension.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "Logging/RetrieveLogChannels.h"
#include "Net/UnrealNetwork.h"
#include "Perception/AISense_Damage.h"

UCombatAttributeSet::UCombatAttributeSet()
{
	InitHealth(100.f);
	InitMaxHealth(100.f);
	InitAttackPower(0.f);
	InitMoveSpeed(600.f);
	InitIncomingDamageMultiplier(1.f);
	InitGuardDamageReduction(0.4f);
	InitAttackSpeedMultiplier(1.f);
}

void UCombatAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UCombatAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCombatAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCombatAttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCombatAttributeSet, MoveSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCombatAttributeSet, IncomingDamageMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCombatAttributeSet, GuardDamageReduction, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCombatAttributeSet, AttackSpeedMultiplier, COND_None, REPNOTIFY_Always);
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
	else if (Attribute == GetGuardDamageReductionAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 1.f);
	}
	else if (Attribute == GetAttackSpeedMultiplierAttribute())
	{
		// 재생속도 0/음수 방지
		NewValue = FMath::Max(0.1f, NewValue);
	}
}

void UCombatAttributeSet::PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		FGameplayTagContainer SpecTags;
		Data.EffectSpec.GetAllAssetTags(SpecTags);

		const float RawDamage = GetIncomingDamage();
		const float FinalDamage = HandleIncomingDamage_Defense(Data, RawDamage, SpecTags);

		SetIncomingDamage(0.f);

		if (FinalDamage > 0.f)
		{
			const float NewHealth = FMath::Clamp(GetHealth() - FinalDamage, 0.f, GetMaxHealth());
			SetHealth(NewHealth);
			BroadcastHitEvent(Data, FinalDamage);
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

void UCombatAttributeSet::OnRep_GuardDamageReduction(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCombatAttributeSet, GuardDamageReduction, OldValue);
}

void UCombatAttributeSet::OnRep_AttackSpeedMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCombatAttributeSet, AttackSpeedMultiplier, OldValue);
}


float UCombatAttributeSet::HandleIncomingDamage_Defense(const FGameplayEffectModCallbackData& Data, float RawDamage, const FGameplayTagContainer& SpecTags)
{
	UAbilitySystemComponent* TargetASC = &Data.Target;
	AActor* TargetActor = TargetASC->GetAvatarActor();
	if (!IsValid(TargetActor))
	{
		return RawDamage;
	}

	const FGameplayEffectContextHandle& Context = Data.EffectSpec.GetEffectContext();

	// 1. PARRY
	if (TargetASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Parrying))
	{
		if (SpecTags.HasTag(RetrieveGameplayTags::Attack_Type_Unblockable))
		{
			return RawDamage;
		}

		AActor* InstigatorActor = Context.GetInstigator();
		AActor* CauserActor = Context.GetEffectCauser();

		// (a) 공격자에게 "패리당함" 발행 → 공격자 GA가 self-stagger + cancel
		if (IsValid(InstigatorActor))
		{
			FGameplayEventData ToAttacker;
			ToAttacker.Instigator = InstigatorActor;
			ToAttacker.Target = TargetActor;
			ToAttacker.OptionalObject = CauserActor;
			ToAttacker.EventTag = RetrieveGameplayTags::GameplayEvent_Parried;
			ToAttacker.TargetTags = SpecTags;
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
				InstigatorActor, RetrieveGameplayTags::GameplayEvent_Parried, ToAttacker);
		}

		// (b) 방어자에게 "패리 성공" 발행 → GA_Guard가 카운터 윈도우 부여 + Cue
		FGameplayEventData ToVictim;
		ToVictim.Instigator = TargetActor;
		ToVictim.Target = TargetActor;
		ToVictim.OptionalObject = InstigatorActor;
		ToVictim.EventTag = RetrieveGameplayTags::GameplayEvent_Parry_Success;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
			TargetActor, RetrieveGameplayTags::GameplayEvent_Parry_Success, ToVictim);

		return 0.f;
	}

	// 2. GUARD
	const bool bGuarding = TargetASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Guarding);

	if (bGuarding)
	{
		if (SpecTags.HasTag(RetrieveGameplayTags::Attack_Property_GuardBreak) ||
			SpecTags.HasTag(RetrieveGameplayTags::Attack_Type_Unblockable))
		{
			FGameplayEventData EventData;
			EventData.Instigator = Data.EffectSpec.GetEffectContext().GetInstigator();
			EventData.Target = TargetActor;
			EventData.EventTag = RetrieveGameplayTags::GameplayEvent_Guard_Broken;

			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
				TargetActor, RetrieveGameplayTags::GameplayEvent_Guard_Broken, EventData);

			return RawDamage;
		}

		return RawDamage * (1.0f - GetGuardDamageReduction());
	}
	
	return RawDamage;
}

void UCombatAttributeSet::BroadcastHitEvent(const struct FGameplayEffectModCallbackData& Data, float DamageDone) const
{
	AActor* AttackerActor = Data.EffectSpec.GetEffectContext().GetInstigator();
	if (IsValid(AttackerActor) == false) return;

	AActor* TargetActor = Data.Target.AbilityActorInfo->AvatarActor.Get();
	if (IsValid(TargetActor) == false) return;

	UAISense_Damage::ReportDamageEvent(
		TargetActor,
		TargetActor,
		AttackerActor,
		DamageDone,
		AttackerActor->GetActorLocation(),
		TargetActor->GetActorLocation());

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

	for (const FGameplayTag& Tag : SourceTags)
	{
		if (Tag.MatchesTag(RetrieveGameplayTags::HitReact_Type))
		{
			EventData.TargetTags.AddTag(Tag);
		}
	}

	EventData.EventTag = AttackerEventTag;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(AttackerActor, AttackerEventTag, EventData);
	if (TargetActor != AttackerActor)
	{
		EventData.EventTag = TargetEventTag;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(TargetActor, TargetEventTag, EventData);
	}
	// 테스트 코드
	UE_LOG(LogRetrieveCombat, Log, TEXT("[HitEvent] AttackerEvent=%s TargetEvent=%s Damage=%.1f Attacker=%s Target=%s"),
		*AttackerEventTag.ToString(),
		*TargetEventTag.ToString(),
		DamageDone,
		*GetNameSafe(AttackerActor),
		*GetNameSafe(TargetActor));
	
	// 공격자 측 연출용 메시지(대미지 플로터 등) GMS로 디커플
	UWorld* World = AttackerActor->GetWorld();
	if (IsValid(World) == false)
	{
		return;
	}
	FRetrieveDamageDealtPayload DamageMsg;
	DamageMsg.Instigator = AttackerActor;
	DamageMsg.Target = TargetActor;
	DamageMsg.DamageAmount = DamageDone;
	DamageMsg.HitEventTag = AttackerEventTag;
	DamageMsg.TargetEventTag = TargetEventTag;
	UGameplayMessageSubsystem::Get(World).BroadcastMessage(RetrieveGameplayTags::Channel_Combat_DamageDealt, DamageMsg);
}
