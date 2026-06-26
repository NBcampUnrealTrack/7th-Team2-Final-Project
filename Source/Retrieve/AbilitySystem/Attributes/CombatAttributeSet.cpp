#include "AbilitySystem/Attributes/CombatAttributeSet.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "GameplayEffectExtension.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "Logging/RetrieveLogChannels.h"
#include "Components/Combat/RetrieveHealthComponent.h"
#include "Combat/RetrieveKnockbackLibrary.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"
#include "Perception/AISense_Damage.h"
#include "Character/RetrieveEnemyCharacter.h"
#include "Components/ActorComponent.h"

UCombatAttributeSet::UCombatAttributeSet()
{
}

void UCombatAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UCombatAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCombatAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCombatAttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCombatAttributeSet, Defense, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCombatAttributeSet, MoveSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCombatAttributeSet, IncomingDamageMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCombatAttributeSet, GuardDamageReduction, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCombatAttributeSet, AttackSpeedMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCombatAttributeSet, Stamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCombatAttributeSet, MaxStamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCombatAttributeSet, StaminaRegenRate, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCombatAttributeSet, Poise, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCombatAttributeSet, MaxPoise, COND_None, REPNOTIFY_Always);
}

void UCombatAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UCombatAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UCombatAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	if (Attribute == GetMaxStaminaAttribute() && GetStamina() > NewValue)
	{
		SetStamina(NewValue);
	}
}

void UCombatAttributeSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetAttackPowerAttribute())
	{
		NewValue = FMath::Max(0.f, NewValue);
	}
	else if (Attribute == GetDefenseAttribute())
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
	else if (Attribute == GetStaminaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxStamina());
	}
	else if (Attribute == GetMaxStaminaAttribute())
	{
		NewValue = FMath::Max(0.f, NewValue);
	}
	else if (Attribute == GetStaminaRegenRateAttribute())
	{
		NewValue = FMath::Max(0.f, NewValue);
	}
	else if (Attribute == GetPoiseAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxPoise());
	}
	else if (Attribute == GetMaxPoiseAttribute())
	{
		NewValue = FMath::Max(0.f, NewValue);
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
			// Health는 IncomingDamage 메타 어트리뷰트→SetHealth로 갱신되므로, SetHealth가 트리거하는
			// HandleHealthChanged엔 GEModData가 없다. → 사망 처리(OnDeathStarted) 전에 EffectContext의
			// instigator/causer를 HealthComponent에 직접 넘겨 "마지막 공격자"를 확정.
			const FGameplayEffectContextHandle& DamageContext = Data.EffectSpec.GetEffectContext();
			if (AActor* DamagedActor = Data.Target.GetAvatarActor())
			{
				if (URetrieveHealthComponent* HealthComp = DamagedActor->FindComponentByClass<URetrieveHealthComponent>())
				{
					HealthComp->NotifyDamageContext(DamageContext.GetInstigator(), DamageContext.GetEffectCauser());
				}
			}

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
	else if (Data.EvaluatedData.Attribute == GetStaminaAttribute())
	{
		SetStamina(FMath::Clamp(GetStamina(), 0.f, GetMaxStamina()));
	}
	else if (Data.EvaluatedData.Attribute == GetMaxStaminaAttribute())
	{
		SetMaxStamina(FMath::Max(0.f, GetMaxStamina()));
		SetStamina(FMath::Clamp(GetStamina(), 0.f, GetMaxStamina()));
	}
	else if (Data.EvaluatedData.Attribute == GetStaminaRegenRateAttribute())
	{
		SetStaminaRegenRate(FMath::Max(0.f, GetStaminaRegenRate()));
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

void UCombatAttributeSet::OnRep_Defense(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCombatAttributeSet, Defense, OldValue);
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

void UCombatAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCombatAttributeSet, Stamina, OldValue);
}

void UCombatAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCombatAttributeSet, MaxStamina, OldValue);
}

void UCombatAttributeSet::OnRep_StaminaRegenRate(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCombatAttributeSet, StaminaRegenRate, OldValue);
}

void UCombatAttributeSet::OnRep_Poise(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCombatAttributeSet, Poise, OldValue);
}

void UCombatAttributeSet::OnRep_MaxPoise(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCombatAttributeSet, MaxPoise, OldValue);
}


float UCombatAttributeSet::HandleIncomingDamage_Defense(const FGameplayEffectModCallbackData& Data, float RawDamage, const FGameplayTagContainer& SpecTags)
{
	UAbilitySystemComponent* TargetASC = &Data.Target;
	AActor* TargetActor = TargetASC->GetAvatarActor();
	if (!IsValid(TargetActor))
	{
		return RawDamage;
	}

	if (TargetASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Boss_PhaseTransition))
	{
		return 0.f;
	}

	const FGameplayEffectContextHandle& Context = Data.EffectSpec.GetEffectContext();
	
	if (TargetASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Parrying)
		&& !SpecTags.HasTag(RetrieveGameplayTags::Attack_Type_Unblockable))
	{
		AActor* InstigatorActor = Context.GetInstigator();
		AActor* CauserActor = Context.GetEffectCauser();

		bool bAttackerIsBoss = false;
		if (const UAbilitySystemComponent* AttackerASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(InstigatorActor))
		{
			bAttackerIsBoss = AttackerASC->HasMatchingGameplayTag(RetrieveGameplayTags::Monster_Type_Boss);
		}

		// 보스 X = 기본 패리 가능 / 보스 O = Parryable 태그가 실린 공격만.
		const bool bParryable = !bAttackerIsBoss || SpecTags.HasTag(RetrieveGameplayTags::Attack_Type_Parryable);
		if (bParryable)
		{
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

			// (b) 방어자에게 "패리 성공" 발행 → GA_Parry/Guard가 카운터 윈도우 부여 + Cue
			FGameplayEventData ToVictim;
			ToVictim.Instigator = TargetActor;
			ToVictim.Target = TargetActor;
			ToVictim.OptionalObject = InstigatorActor;
			ToVictim.EventTag = RetrieveGameplayTags::GameplayEvent_Parry_Success;
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
				TargetActor, RetrieveGameplayTags::GameplayEvent_Parry_Success, ToVictim);

			return 0.f;
		}
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

		const float DamageAfterGuard = RawDamage * (1.0f - GetGuardDamageReduction());
		return FMath::Max(0.0f, DamageAfterGuard - GetDefense());
	}
	
	return FMath::Max(0.0f, RawDamage - GetDefense());
}

void UCombatAttributeSet::BroadcastHitEvent(const struct FGameplayEffectModCallbackData& Data, float DamageDone) const
{
	const FGameplayEffectContextHandle& Context = Data.EffectSpec.GetEffectContext();
	AActor* AttackerActor = Context.GetInstigator();
	if (!IsValid(AttackerActor))
	{
		AttackerActor = Context.GetEffectCauser();
	}
	if (!IsValid(AttackerActor))
	{
		if (const UObject* SourceObject = Context.GetSourceObject())
		{
			if (const AActor* SourceActor = Cast<AActor>(SourceObject))
			{
				AttackerActor = const_cast<AActor*>(SourceActor);
			}
			else if (const UActorComponent* SourceComponent = Cast<UActorComponent>(SourceObject))
			{
				AttackerActor = SourceComponent->GetOwner();
			}
		}
	}
	if (IsValid(AttackerActor) == false) return;

	AActor* TargetActor = Data.Target.AbilityActorInfo->AvatarActor.Get();
	if (IsValid(TargetActor) == false) return;

	// 데미지 GE가 SetByCaller로 넉백 강도를 실었으면 공격자→피격자 방향으로 자동 넉백.
	// FinalDamage>0 경로(PostGameplayEffectExecute)에서만 호출되므로 가드/패리/무적 시엔 자동 스킵된다.
	const float KbStrength = Data.EffectSpec.GetSetByCallerMagnitude(
		RetrieveGameplayTags::Data_Knockback_Strength, /*bWarnIfNotFound=*/false, /*DefaultIfNotFound=*/0.f);
	if (KbStrength > 0.f)
	{
		if (ACharacter* TargetCharacter = Cast<ACharacter>(TargetActor))
		{
			const float KbUp = Data.EffectSpec.GetSetByCallerMagnitude(
				RetrieveGameplayTags::Data_Knockback_UpwardStrength, false, 0.f);
			const float CancelTargetActionsValue = Data.EffectSpec.GetSetByCallerMagnitude(
				RetrieveGameplayTags::Data_Knockback_CancelTargetActions,
				false, 0.f);
			
			FRetrieveKnockbackParams Params;
			Params.Strength = KbStrength;
			Params.UpwardStrength = KbUp;
			Params.bCancelTargetActions = CancelTargetActionsValue > 0.f;;
			
			URetrieveKnockbackLibrary::ApplyPlanarKnockbackFromActor(
				TargetCharacter, AttackerActor, Params);
		}
	}

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
		if (Tag.MatchesTag(RetrieveGameplayTags::HitReact_Type) ||
			Tag.MatchesTag(RetrieveGameplayTags::Element) ||
			Tag.MatchesTag(RetrieveGameplayTags::Attack_Type) ||
			Tag.MatchesTag(RetrieveGameplayTags::Attack_Property))
		{
			EventData.TargetTags.AddTag(Tag);
		}
	}

	EventData.EventTag = AttackerEventTag;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(AttackerActor, AttackerEventTag, EventData);

	// Burn 등 지속 데미지(DoT)는 데미지만 적용하고 피격 반응(움찔/몽타주)은 발생시키지 않는다.
	// 틱마다 Hit 이벤트가 나가면 매 틱 Flinch가 걸리므로 타겟 Hit 이벤트만 건너뛴다.
	const bool bIsDamageOverTime =
		AttackerEventTag.MatchesTag(RetrieveGameplayTags::GameplayEvent_Attack_HitSuccess_Burn);

	if (TargetActor != AttackerActor && !bIsDamageOverTime)
	{
		EventData.EventTag = TargetEventTag;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(TargetActor, TargetEventTag, EventData);
	}

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
