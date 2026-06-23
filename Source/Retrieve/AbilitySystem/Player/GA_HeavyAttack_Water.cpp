#include "AbilitySystem/Player/GA_HeavyAttack_Water.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Combat/RetrieveCombatTypes.h"

UGA_HeavyAttack_Water::UGA_HeavyAttack_Water()
{
	ActivationRequiredTags.AddTag(RetrieveGameplayTags::Element_Water);

	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_HeavyAttack);
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_HeavyAttack_Water);
	Tags.AddTag(RetrieveGameplayTags::Ability_Type_Attack);
	SetAssetTags(Tags);
	
	ActivationOwnedTags.AddTag(RetrieveGameplayTags::Animation_Lock_Movement);
	ActivationOwnedTags.AddTag(RetrieveGameplayTags::Animation_Lock_Rotation);
}

void UGA_HeavyAttack_Water::ExecuteHeavyEffect(const FGameplayTag& /*ConsumedElement*/)
{
	ExecuteOwnerCue(RetrieveGameplayTags::GameplayCue_HeavyAttack_Water);
	ApplyAoEColdDamage();
	PlayHeavyMontageThenEnd();
}

void UGA_HeavyAttack_Water::ApplyAoEColdDamage()
{
	if (!HasAuthority(&GetCurrentActivationInfoRef()))
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(SourceASC) || !IsValid(AvatarActor) || !IsValid(DamageEffectClass))
	{
		return;
	}

	UWorld* World = AvatarActor->GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	const FVector Origin = AvatarActor->GetActorLocation();

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel1);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GA_HeavyAttack_Water), false, AvatarActor);

	TArray<FOverlapResult> Overlaps;
	const bool bHit = World->OverlapMultiByObjectType(
		Overlaps, Origin, FQuat::Identity, ObjectQueryParams, FCollisionShape::MakeSphere(SweepRadius), QueryParams);

	if (bDebugDrawSweep)
	{
		DrawDebugSphere(World, Origin, SweepRadius, 16, bHit ? FColor::Cyan : FColor::Red, false, 1.5f);
	}

	if (!bHit)
	{
		return;
	}

	TSet<AActor*> HitActors;
	for (const FOverlapResult& Ov : Overlaps)
	{
		AActor* TargetActor = Ov.GetActor();
		if (!IsValid(TargetActor) || TargetActor == AvatarActor || HitActors.Contains(TargetActor))
		{
			continue;
		}
		HitActors.Add(TargetActor);

		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
		if (!IsValid(TargetASC))
		{
			continue;
		}

		FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
		Ctx.AddInstigator(AvatarActor, AvatarActor);
		Ctx.AddSourceObject(this);

		// 1) 피해 적용
		FGameplayEffectSpecHandle DamageSpec = SourceASC->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(), Ctx);
		if (DamageSpec.IsValid() && DamageSpec.Data.IsValid())
		{
			DamageSpec.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Damage_Mul, DamageMultiplier);
			if (KnockbackStrength > 0.f)
			{
				DamageSpec.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Knockback_Strength, KnockbackStrength);
				DamageSpec.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Knockback_UpwardStrength, KnockbackUpwardStrength);
			}
			DamageSpec.Data->AddDynamicAssetTag(RetrieveGameplayTags::Attack_Type_Heavy);
			DamageSpec.Data->AddDynamicAssetTag(RetrieveGameplayTags::Attack_Property_GuardBreak);
			
			AddCombatTagsToDamageSpec(
				*DamageSpec.Data.Get(),
				RetrieveGameplayTags::Element_Water,
				RetrieveGameplayTags::Attack_Type_Heavy,
				RetrieveGameplayTags::Attack_Property_GuardBreak);
			DamageSpec.Data->AddDynamicAssetTag(RetrieveGameplayTags::GameplayEvent_Hit_Heavy);

			SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpec.Data.Get(), TargetASC);
		}

		// 2) Cold(빙결) 상태 부여
		if (IsValid(ColdEffectClass))
		{
			FGameplayEffectSpecHandle ColdSpec = SourceASC->MakeOutgoingSpec(ColdEffectClass, GetAbilityLevel(), Ctx);
			if (ColdSpec.IsValid() && ColdSpec.Data.IsValid())
			{
				SourceASC->ApplyGameplayEffectSpecToTarget(*ColdSpec.Data.Get(), TargetASC);
			}
		}
	}
}
