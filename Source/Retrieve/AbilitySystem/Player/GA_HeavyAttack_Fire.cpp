#include "AbilitySystem/Player/GA_HeavyAttack_Fire.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "GameplayTags/RetrieveGameplayTags.h"

UGA_HeavyAttack_Fire::UGA_HeavyAttack_Fire()
{
	ActivationRequiredTags.AddTag(RetrieveGameplayTags::Element_Fire);
	
	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_HeavyAttack);
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_HeavyAttack_Fire);
	SetAssetTags(Tags);
	
	ActivationOwnedTags.AddTag(RetrieveGameplayTags::Animation_Lock_Movement);
	ActivationOwnedTags.AddTag(RetrieveGameplayTags::Animation_Lock_Rotation);
}

void UGA_HeavyAttack_Fire::ExecuteHeavyEffect(const FGameplayTag& /*ConsumedElement*/)
{
	ExecuteOwnerCue(RetrieveGameplayTags::GameplayCue_HeavyAttack_Fire);
	ApplyRadialDamage();
	PlayHeavyMontageThenEnd();
}

void UGA_HeavyAttack_Fire::ApplyRadialDamage()
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
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GA_HeavyAttack_Fire), false, AvatarActor);

	TArray<FOverlapResult> Overlaps;
	const bool bHit = World->OverlapMultiByObjectType(
		Overlaps, Origin, FQuat::Identity, ObjectQueryParams, FCollisionShape::MakeSphere(SweepRadius), QueryParams);

	if (bDebugDrawSweep)
	{
		DrawDebugSphere(World, Origin, SweepRadius, 16, bHit ? FColor::Green : FColor::Red, false, 1.5f);
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

		FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(), Ctx);
		if (!Spec.IsValid() || !Spec.Data.IsValid())
		{
			continue;
		}

		Spec.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Damage_Mul, DamageMultiplier);
		Spec.Data->AddDynamicAssetTag(RetrieveGameplayTags::Attack_Type_Heavy);
		Spec.Data->AddDynamicAssetTag(RetrieveGameplayTags::Attack_Property_GuardBreak);
		Spec.Data->AddDynamicAssetTag(RetrieveGameplayTags::GameplayEvent_Hit_Heavy);

		SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	}
}
