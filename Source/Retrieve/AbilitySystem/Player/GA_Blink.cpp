#include "AbilitySystem/Player/GA_Blink.h"

#include "Components/CapsuleComponent.h"
#include "Components/Player/RetrieveHeroComponent.h"
#include "Components/Player/WeaponComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameplayTags/RetrieveGameplayTags.h"

UGA_Blink::UGA_Blink()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_Dash);
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_Blink);
	SetAssetTags(Tags);

	bBlockActivationWhileAirborne = true;

	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dead);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Knockdown);
	
	CancelAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Type_Attack);
}

bool UGA_Blink::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	
	const AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const UWeaponComponent* WeaponComp = AvatarActor ? AvatarActor->FindComponentByClass<UWeaponComponent>() : nullptr;
	return WeaponComp && WeaponComp->GetWeaponDataRef().WeaponTypeTag == RetrieveGameplayTags::Weapon_Type_Staff;
}

void UGA_Blink::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ACharacter* Character = Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!Character || !World)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FVector Start = Character->GetActorLocation();
	FVector Dir = ResolveBlinkDirection(ActorInfo);
	if (Dir.IsNearlyZero())
	{
		Dir = Character->GetActorForwardVector();
	}
	Dir.Z = 0.f;
	Dir = Dir.GetSafeNormal();

	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const float MyRadius = Capsule ? Capsule->GetScaledCapsuleRadius() : 34.f;
	const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 88.f;
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(MyRadius, HalfHeight);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GA_Blink), false, Character);

	float MaxDist = BlinkDistance;

	// 벽만 막기(적 Pawn은 통과)
	{
		FCollisionObjectQueryParams WorldObjects;
		WorldObjects.AddObjectTypesToQuery(ECC_WorldStatic);
		WorldObjects.AddObjectTypesToQuery(ECC_WorldDynamic);
		FHitResult WallHit;
		if (World->SweepSingleByObjectType(WallHit, Start, Start + Dir * MaxDist, FQuat::Identity, WorldObjects, Shape, Params))
		{
			MaxDist = FMath::Max(0.f, WallHit.Distance);
		}
	}

	// 도착지에 적이 겹치면 그 앞으로 당김(경로 중간의 적은 통과)
	{
		FCollisionObjectQueryParams PawnObjects;
		PawnObjects.AddObjectTypesToQuery(ECC_Pawn);
		TArray<FOverlapResult> Overlaps;
		if (World->OverlapMultiByObjectType(Overlaps, Start + Dir * MaxDist, FQuat::Identity, PawnObjects, Shape, Params))
		{
			float ClosestAlong = MaxDist;
			bool bFoundPawn = false;
			for (const FOverlapResult& Overlap : Overlaps)
			{
				const AActor* PawnActor = Overlap.GetActor();
				if (!PawnActor || PawnActor == Character)
				{
					continue;
				}

				float EnemyRadius = MyRadius;
				if (const ACharacter* EnemyChar = Cast<ACharacter>(PawnActor))
				{
					if (const UCapsuleComponent* EnemyCapsule = EnemyChar->GetCapsuleComponent())
					{
						EnemyRadius = EnemyCapsule->GetScaledCapsuleRadius();
					}
				}

				const float DistAlong = FVector::DotProduct(PawnActor->GetActorLocation() - Start, Dir);
				const float StandoffDist = DistAlong - (MyRadius + EnemyRadius + EnemyStandoffGap);
				if (StandoffDist < ClosestAlong)
				{
					ClosestAlong = StandoffDist;
					bFoundPawn = true;
				}
			}
			if (bFoundPawn)
			{
				MaxDist = FMath::Clamp(ClosestAlong, 0.f, MaxDist);
			}
		}
	}

	const FVector Target = Start + Dir * MaxDist;

	// 즉시 순간이동
	Character->SetActorLocation(Target, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);

	if (bDebugDraw)
	{
		DrawDebugLine(World, Start, Target, FColor::Cyan, false, 1.f, 0, 2.f);
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

FVector UGA_Blink::ResolveBlinkDirection(const FGameplayAbilityActorInfo* ActorInfo) const
{
	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!IsValid(AvatarActor))
	{
		return FVector::ZeroVector;
	}

	if (const URetrieveHeroComponent* Hero = URetrieveHeroComponent::FindHeroComponent(AvatarActor))
	{
		FVector Cached = Hero->GetCachedMoveInputDirection();
		Cached.Z = 0.f;
		if (!Cached.IsNearlyZero())
		{
			return Cached.GetSafeNormal();
		}
	}

	FVector Forward = AvatarActor->GetActorForwardVector();
	Forward.Z = 0.f;
	return Forward.GetSafeNormal();
}
