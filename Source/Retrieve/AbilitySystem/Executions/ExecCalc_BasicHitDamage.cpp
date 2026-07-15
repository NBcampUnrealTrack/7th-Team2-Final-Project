#include "AbilitySystem/Executions/ExecCalc_BasicHitDamage.h"

#include "AbilitySystem/Attributes/CombatAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Logging/RetrieveLogChannels.h"

namespace RetrieveBasicHitDamageStatics
{
	struct FCaptureDefs
	{
		DECLARE_ATTRIBUTE_CAPTUREDEF(AttackPower);
		DECLARE_ATTRIBUTE_CAPTUREDEF(IncomingDamageMultiplier);
		DECLARE_ATTRIBUTE_CAPTUREDEF(NormalAttackDamageMultiplier);
		DECLARE_ATTRIBUTE_CAPTUREDEF(HeavyAttackDamageMultiplier);
		DECLARE_ATTRIBUTE_CAPTUREDEF(ElementalDamageMultiplier);
		DECLARE_ATTRIBUTE_CAPTUREDEF(CriticalChance);
		DECLARE_ATTRIBUTE_CAPTUREDEF(CriticalDamageMultiplier);
		DECLARE_ATTRIBUTE_CAPTUREDEF(OutgoingDamageMultiplier);

		FCaptureDefs()
		{
			DEFINE_ATTRIBUTE_CAPTUREDEF(UCombatAttributeSet, AttackPower, Source, true);
			DEFINE_ATTRIBUTE_CAPTUREDEF(UCombatAttributeSet, IncomingDamageMultiplier, Target, false);
			DEFINE_ATTRIBUTE_CAPTUREDEF(UCombatAttributeSet, NormalAttackDamageMultiplier, Source, true);
			DEFINE_ATTRIBUTE_CAPTUREDEF(UCombatAttributeSet, HeavyAttackDamageMultiplier, Source, true);
			DEFINE_ATTRIBUTE_CAPTUREDEF(UCombatAttributeSet, ElementalDamageMultiplier, Source, true);
			DEFINE_ATTRIBUTE_CAPTUREDEF(UCombatAttributeSet, CriticalChance, Source, true);
			DEFINE_ATTRIBUTE_CAPTUREDEF(UCombatAttributeSet, CriticalDamageMultiplier, Source, true);
			DEFINE_ATTRIBUTE_CAPTUREDEF(UCombatAttributeSet, OutgoingDamageMultiplier, Source, true);
		}
	};

	static const FCaptureDefs& Get()
	{
		static FCaptureDefs Defs;
		return Defs;
	}
}

UExecCalc_BasicHitDamage::UExecCalc_BasicHitDamage()
{
	RelevantAttributesToCapture.Add(RetrieveBasicHitDamageStatics::Get().AttackPowerDef);
    RelevantAttributesToCapture.Add(RetrieveBasicHitDamageStatics::Get().IncomingDamageMultiplierDef);
	RelevantAttributesToCapture.Add(RetrieveBasicHitDamageStatics::Get().NormalAttackDamageMultiplierDef);
	RelevantAttributesToCapture.Add(RetrieveBasicHitDamageStatics::Get().HeavyAttackDamageMultiplierDef);
	RelevantAttributesToCapture.Add(RetrieveBasicHitDamageStatics::Get().ElementalDamageMultiplierDef);
	RelevantAttributesToCapture.Add(RetrieveBasicHitDamageStatics::Get().CriticalChanceDef);
	RelevantAttributesToCapture.Add(RetrieveBasicHitDamageStatics::Get().CriticalDamageMultiplierDef);
	RelevantAttributesToCapture.Add(RetrieveBasicHitDamageStatics::Get().OutgoingDamageMultiplierDef);
}

void UExecCalc_BasicHitDamage::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	
	const FGameplayEffectContextHandle& Ctx = Spec.GetContext();
	AActor* SourceActor = Ctx.GetOriginalInstigator();
	AActor* TargetActor = ExecutionParams.GetTargetAbilitySystemComponent() ? 
		ExecutionParams.GetTargetAbilitySystemComponent()->GetAvatarActor() : nullptr;

	FAggregatorEvaluateParameters EvalParams;
	EvalParams.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	EvalParams.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();
	
	const auto& CaptureDefs = RetrieveBasicHitDamageStatics::Get();

	float AttackPower = 0.0f;
	if (!ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(CaptureDefs.AttackPowerDef, EvalParams, AttackPower))
	{
		UE_LOG(LogRetrieveCombat, Warning, 
			TEXT("[ExecCalc_BasicHitDamage] Failed to capture AttackPower attribute! Source=%s, Target=%s. Fallback to 0.0"), 
			*GetNameSafe(SourceActor), *GetNameSafe(TargetActor));
	}

	AttackPower = FMath::Max(0.0f, AttackPower);
	
	float Mul = Spec.GetSetByCallerMagnitude(
		RetrieveGameplayTags::Data_Damage_Mul,
		/* bWarnIfNotFound = */ false,
		/* DefaultIfNotFound = */ 1.0f);
	
	if (Mul < 0.0f)
	{
		UE_LOG(LogRetrieveCombat, Warning, 
			TEXT("[ExecCalc_BasicHitDamage] Negative damage multiplier received: %f (Source=%s). Clamped to 0.0"), 
			Mul, *GetNameSafe(SourceActor));
		Mul = 0.0f;
	}

	float IncomingMul = 1.0f;
	if (!ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(CaptureDefs.IncomingDamageMultiplierDef, EvalParams, IncomingMul))
	{
		UE_LOG(LogRetrieveCombat, Warning,
			TEXT("[ExecCalc_BasicHitDamage] Failed to capture IncomingDamageMultiplier attribute! Source=%s, Target=%s. Fallback to 0.0"),
			*GetNameSafe(SourceActor), *GetNameSafe(TargetActor));
	}

	IncomingMul = FMath::Max(0.f, IncomingMul);

	// 핵심 데미지 공식 산출
	float Damage = AttackPower * Mul * IncomingMul;

	// ---- 빌드 스탯: 태그 조건부 배율 + 크리티컬 ----
	// 환경 데미지(낙하 등)는 공격자 버프의 영향을 받지 않는다.
	const FGameplayTagContainer* SpecTags = Spec.CapturedSourceTags.GetAggregatedTags();
	bool bCritical = false;
	if (SpecTags && !SpecTags->HasTag(RetrieveGameplayTags::Attack_Type_Environmental))
	{
		auto CaptureMul = [&](const FGameplayEffectAttributeCaptureDefinition& Def, float Default) -> float
		{
			float Value = Default;
			if (!ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(Def, EvalParams, Value))
			{
				Value = Default;
			}
			return FMath::Max(0.f, Value);
		};

		Damage *= CaptureMul(CaptureDefs.OutgoingDamageMultiplierDef, 1.f);

		if (SpecTags->HasTag(RetrieveGameplayTags::Attack_Type_Normal))
		{
			Damage *= CaptureMul(CaptureDefs.NormalAttackDamageMultiplierDef, 1.f);
		}
		if (SpecTags->HasTag(RetrieveGameplayTags::Attack_Type_Heavy))
		{
			Damage *= CaptureMul(CaptureDefs.HeavyAttackDamageMultiplierDef, 1.f);
		}
		if (SpecTags->HasTag(RetrieveGameplayTags::Element_Fire) ||
			SpecTags->HasTag(RetrieveGameplayTags::Element_Water) ||
			SpecTags->HasTag(RetrieveGameplayTags::Element_Wind))
		{
			Damage *= CaptureMul(CaptureDefs.ElementalDamageMultiplierDef, 1.f);
		}

		// 크리티컬 — 서버에서만 실행되므로 랜덤 안전
		const float CritChance = FMath::Clamp(CaptureMul(CaptureDefs.CriticalChanceDef, 0.f), 0.f, UCombatAttributeSet::CriticalChanceCap);
		if (CritChance > 0.f && FMath::FRand() < CritChance)
		{
			Damage *= FMath::Max(1.f, CaptureMul(CaptureDefs.CriticalDamageMultiplierDef, 1.5f));
			bCritical = true;

			// 스펙에 크리티컬 태그를 실어 PostGameplayEffectExecute/플로터가 강조 표시할 수 있게 한다.
			if (FGameplayEffectSpec* MutableSpec = ExecutionParams.GetOwningSpecForPreExecuteMod())
			{
				MutableSpec->AddDynamicAssetTag(RetrieveGameplayTags::Attack_Property_Critical);
			}
		}
	}

	const float FinalDamage = FMath::Max(1.0f, Damage); // 최소 데미지 1 안전 보장

	UE_LOG(LogRetrieveCombat, Log, TEXT("[ExecCalc] Success! Source=%s -> Target=%s | ATK=%.1f, Mul=%.1f, Crit=%d -> FinalDamage=%.1f"),
		*GetNameSafe(SourceActor), *GetNameSafe(TargetActor), AttackPower, Mul, bCritical ? 1 : 0, FinalDamage);
	
	OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
		UCombatAttributeSet::GetIncomingDamageAttribute(),
		EGameplayModOp::Additive,
		FinalDamage));
}
