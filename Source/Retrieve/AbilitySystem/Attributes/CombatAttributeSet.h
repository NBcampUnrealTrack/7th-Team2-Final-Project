#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/RetrieveAttributeSet.h"
#include "CombatAttributeSet.generated.h"

/**
 * 모든 전투 가능한 캐릭터(소버린, 적, 보스)가 공유합니다.
 * 무기 및 스킬 데미지는 어트리뷰트셋 외부에서 계산됩니다.
 */

UCLASS()
class RETRIEVE_API UCombatAttributeSet : public URetrieveAttributeSet
{
	GENERATED_BODY()

public:
	UCombatAttributeSet();

	/** MoveSpeed Attribute의 기준값(cm/s). 이 값이 ALS DA 속도 그대로(=배율 1.0)에 대응합니다. */
	static constexpr float ReferenceMoveSpeed = 600.f;

	/** 방어력 비율 감쇄 상수(K). 받는 피해 = Damage * K/(K+Defense).
	 *  Defense가 이 값과 같아지면 피해가 정확히 절반이 된다. */
	static constexpr float DefenseConstant = 100.f;

	/** 크리티컬 확률 상한. 장비/공명 중첩으로도 이 이상 올라가지 않는다(폭주 방지). */
	static constexpr float CriticalChanceCap = 0.6f;

	/** 흡혈 비율 상한(준 데미지 대비 회복 %). */
	static constexpr float LifeStealRatioCap = 0.3f;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, MaxHealth)
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AttackPower)
	FGameplayAttributeData AttackPower;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, AttackPower)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Defense)
	FGameplayAttributeData Defense;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, Defense)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MoveSpeed)
	FGameplayAttributeData MoveSpeed;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, MoveSpeed)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_IncomingDamageMultiplier)
	FGameplayAttributeData IncomingDamageMultiplier;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, IncomingDamageMultiplier)

	UPROPERTY(BlueprintReadOnly)
	FGameplayAttributeData IncomingDamage;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, IncomingDamage)

	UPROPERTY(BlueprintReadOnly)
	FGameplayAttributeData IncomingHealing;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, IncomingHealing)
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_GuardDamageReduction)
	FGameplayAttributeData GuardDamageReduction;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, GuardDamageReduction)

	/** 공격 몽타주 재생 속도 배율. 기준값 1.0 = 등속. GA_Attack의 PlayRate로 사용됨. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AttackSpeedMultiplier)
	FGameplayAttributeData AttackSpeedMultiplier;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, AttackSpeedMultiplier)
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Stamina)
	FGameplayAttributeData Stamina;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, Stamina)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxStamina)
	FGameplayAttributeData MaxStamina;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, MaxStamina)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Poise)
	FGameplayAttributeData Poise;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, Poise)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxPoise)
	FGameplayAttributeData MaxPoise;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, MaxPoise)

	// ---- 빌드 스탯 (장비 특수효과/원소 공명/세트가 GE로 증감. 전부 중립 기본값 = 미부여 시 기존 밸런스 불변) ----

	/** 일반공격(Attack.Type.Normal) 데미지 배율. 기본 1.0. ExecCalc에서 소비. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_NormalAttackDamageMultiplier)
	FGameplayAttributeData NormalAttackDamageMultiplier;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, NormalAttackDamageMultiplier)

	/** 강공격(Attack.Type.Heavy) 데미지 배율. 기본 1.0. ExecCalc에서 소비. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HeavyAttackDamageMultiplier)
	FGameplayAttributeData HeavyAttackDamageMultiplier;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, HeavyAttackDamageMultiplier)

	/** 원소(Element.Fire/Water/Wind) 실린 공격 데미지 배율. 기본 1.0. ExecCalc에서 소비. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ElementalDamageMultiplier)
	FGameplayAttributeData ElementalDamageMultiplier;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, ElementalDamageMultiplier)

	/** 치명타 확률(0~CriticalChanceCap). 기본 0. ExecCalc 서버 굴림. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CriticalChance)
	FGameplayAttributeData CriticalChance;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, CriticalChance)

	/** 치명타 데미지 배율. 기본 1.5, 최소 1.0. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CriticalDamageMultiplier)
	FGameplayAttributeData CriticalDamageMultiplier;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, CriticalDamageMultiplier)

	/** 흡혈 비율(준 데미지 대비, 0~LifeStealRatioCap). 기본 0. 피격자 PostGameplayEffectExecute에서 공격자 회복. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_LifeStealRatio)
	FGameplayAttributeData LifeStealRatio;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, LifeStealRatio)

	/** 원소 게이지 획득 배율. 기본 1.0. ElementGaugeComponent::AddCharge에서 소비. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ElementChargeGainMultiplier)
	FGameplayAttributeData ElementChargeGainMultiplier;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, ElementChargeGainMultiplier)

	/** 주는 데미지 전체 배율(범용 버프용). 기본 1.0. ExecCalc에서 소비. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_OutgoingDamageMultiplier)
	FGameplayAttributeData OutgoingDamageMultiplier;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, OutgoingDamageMultiplier)

	/** 스태미너 자연 회복 속도 배율. 기본 1.0(중립). 회복 base는 URetrieveStaminaSettings 소유,
	 *  이 배율만 세트/버프 GE로 증감해 StaminaComponent 회복 계산에 곱해진다. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_StaminaRegenMultiplier)
	FGameplayAttributeData StaminaRegenMultiplier;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, StaminaRegenMultiplier)

private:
	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;

	UFUNCTION() void OnRep_Health(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_AttackPower(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Defense(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MoveSpeed(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_IncomingDamageMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_GuardDamageReduction(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_AttackSpeedMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Stamina(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxStamina(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Poise(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxPoise(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_NormalAttackDamageMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_HeavyAttackDamageMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ElementalDamageMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_CriticalChance(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_CriticalDamageMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_LifeStealRatio(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ElementChargeGainMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_OutgoingDamageMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_StaminaRegenMultiplier(const FGameplayAttributeData& OldValue);

	// 방어 분기 단일 진입점
	float HandleIncomingDamage_Defense(const FGameplayEffectModCallbackData& Data, float RawDamage, const FGameplayTagContainer& SpecTags);
	// 방어력 비율 감쇄: 받는 피해 = Damage * DefenseConstant/(DefenseConstant + Defense), 최소 1 보장
	float ApplyDefenseMitigation(float Damage) const;
	// 데미지 적용 후 카메라/플로터/리액션 시스템에 알릴 이벤트 발행
	void BroadcastHitEvent(const struct FGameplayEffectModCallbackData& Data, float DamageDone) const;
	// 공격자의 LifeStealRatio에 따라 최종 데미지 비율만큼 공격자 체력 회복 (환경/DoT/자기피격 제외)
	void ApplyLifeStealToSource(const struct FGameplayEffectModCallbackData& Data, float DamageDone, const FGameplayTagContainer& SpecTags) const;
};
