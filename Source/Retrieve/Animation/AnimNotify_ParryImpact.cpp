#include "Animation/AnimNotify_ParryImpact.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Combat/RetrieveCombatTypes.h"
#include "Components/Player/WeaponComponent.h"
#include "Data/WeaponAttackDefinition.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "GameplayTags/RetrieveGameplayTags.h"

FString UAnimNotify_ParryImpact::GetNotifyName_Implementation() const
{
	return TEXT("ParryImpact");
}

void UAnimNotify_ParryImpact::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* Owner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;
	if (!IsValid(Owner) || Owner->GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	URetrieveAbilitySystemComponent* SourceASC = Cast<URetrieveAbilitySystemComponent>(
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner));
	if (!IsValid(SourceASC))
	{
		return;
	}

	AActor* Target = SourceASC->GetPendingCounterTarget();
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
	if (!IsValid(Target) || !IsValid(TargetASC))
	{
		return;
	}

	const UWeaponComponent* Weapon = Owner->FindComponentByClass<UWeaponComponent>();
	const UWeaponAttackDefinition* Def = Weapon ? Weapon->GetWeaponDataRef().AttackComboDefinition.LoadSynchronous() : nullptr;
	if (!IsValid(Def))
	{
		return;
	}
	const FWeaponParryData& Parry = Def->Parry;

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddInstigator(Owner, Owner);

	// 타격: 소량 데미지 + 넉백 + 피드백 태그 → 타격음/히트리액션/카메라셰이크/플로터 자동.
	if (Parry.SuccessDamageEffect && Parry.SuccessDamageMultiplier > 0.f)
	{
		FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(Parry.SuccessDamageEffect, 1.f, Context);
		if (Spec.IsValid() && Spec.Data.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Damage_Mul, Parry.SuccessDamageMultiplier);

			if (Parry.SuccessKnockback.Strength > 0.f)
			{
				Spec.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Knockback_Strength, Parry.SuccessKnockback.Strength);
				Spec.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Knockback_UpwardStrength, Parry.SuccessKnockback.UpwardStrength);
				Spec.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Knockback_CancelTargetActions, 1.f);
			}

			AddCombatTagsToDamageSpec(*Spec.Data.Get(), FGameplayTag(), RetrieveGameplayTags::Attack_Type_Normal,
				FGameplayTag(), HitReactTypeToTag(Parry.SuccessHitReactType));

			const FGameplayTag HitSuccessTag = Parry.SuccessHitSuccessFeedbackTag.IsValid()
				? Parry.SuccessHitSuccessFeedbackTag : RetrieveGameplayTags::GameplayEvent_Attack_HitSuccess_Heavy;
			const FGameplayTag TargetHitTag = Parry.SuccessTargetHitFeedbackTag.IsValid()
				? Parry.SuccessTargetHitFeedbackTag : RetrieveGameplayTags::GameplayEvent_Hit_Heavy;
			Spec.Data->AddDynamicAssetTag(HitSuccessTag);
			Spec.Data->AddDynamicAssetTag(TargetHitTag);

			SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
		}
	}

	// 스태거는 UGA_ParryBase::ApplyParryStagger로 이관. 이 노티는 몽타주 중간이라 카운터 수용창(패리 성립)과
	// 어긋나고, 빠른 카운터가 몽타주를 블렌드아웃시키면 노티가 사라져 스태거가 유실됐다.
}
