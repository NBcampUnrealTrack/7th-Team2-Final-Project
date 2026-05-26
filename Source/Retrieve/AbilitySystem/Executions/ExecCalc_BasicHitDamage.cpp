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

		FCaptureDefs()
		{
			DEFINE_ATTRIBUTE_CAPTUREDEF(UCombatAttributeSet, AttackPower, Source, true);
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

	// 핵심 데미지 공식 산출
	const float RawDamage = AttackPower * Mul;
	const float FinalDamage = FMath::Max(1.0f, RawDamage); // 최소 데미지 1 안전 보장
	
	UE_LOG(LogRetrieveCombat, Log, TEXT("[ExecCalc] Success! Source=%s -> Target=%s | ATK=%.1f, Mul=%.1f -> FinalDamage=%.1f"),
		*GetNameSafe(SourceActor), *GetNameSafe(TargetActor), AttackPower, Mul, FinalDamage);
	
	OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
		UCombatAttributeSet::GetIncomingDamageAttribute(),
		EGameplayModOp::Additive,
		FinalDamage));
}
