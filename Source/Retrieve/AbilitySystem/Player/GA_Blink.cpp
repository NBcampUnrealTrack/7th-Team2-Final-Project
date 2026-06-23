#include "AbilitySystem/Player/GA_Blink.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/Player/RetrieveHeroComponent.h"
#include "Components/Player/WeaponComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
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
	return WeaponComp &&
		WeaponComp->GetWeaponDataRef().WeaponTypeTag == RetrieveGameplayTags::Weapon_Type_Staff;
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
	
	StopArrivalMontage(Character);

	FVector Dir = ResolveBlinkDirection(ActorInfo);
	if (Dir.IsNearlyZero())
	{
		Dir = Character->GetActorForwardVector();
	}
	Dir.Z = 0.f;
	Dir = Dir.GetSafeNormal();

	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const float Radius = Capsule ? Capsule->GetScaledCapsuleRadius() : 34.f;
	const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 88.f;

	const FVector Start = Character->GetActorLocation();
	const float DashDist = ComputeDashDistance(Character, Start, Dir, Radius, HalfHeight);
	
	if (DashDist < 1.f)
	{
		PlayArrivalMontage(Character);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}
	
	SetPawnCollisionIgnored(Character, true);

	const FVector Target(Start.X + Dir.X * DashDist, Start.Y + Dir.Y * DashDist, Start.Z);
	const float Duration = FMath::Max(0.02f, DashDist / DashSpeed);

	DashTask = UAbilityTask_ApplyRootMotionMoveToForce::ApplyRootMotionMoveToForce(
		this, FName("BlinkDash"), Target, Duration,
		/*bSetNewMovementMode=*/false, MOVE_Walking,
		/*bRestrictSpeedToExpected=*/true, /*PathOffsetCurve=*/nullptr,
		ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector, /*ClampVelocityOnFinish=*/0.f);

	if (!DashTask)
	{
		SetPawnCollisionIgnored(Character, false);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	DashTask->OnTimedOut.AddDynamic(this, &ThisClass::OnDashFinished);
	DashTask->OnTimedOutAndDestinationReached.AddDynamic(this, &ThisClass::OnDashFinished);
	DashTask->ReadyForActivation();

	PlayArrivalMontage(Character);
}

float UGA_Blink::ComputeDashDistance(ACharacter* Character, const FVector& Start, const FVector& Dir, float CapsuleRadius, float CapsuleHalfHeight) const
{
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!World)
	{
		return BlinkDistance;
	}
	
	const FVector NominalEnd(Start.X + Dir.X * BlinkDistance, Start.Y + Dir.Y * BlinkDistance, Start.Z);

	const FCollisionShape Shape = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);
	FCollisionObjectQueryParams PawnObjects;
	PawnObjects.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GA_Blink_Dest), false, Character);

	TArray<FOverlapResult> Overlaps;
	if (!World->OverlapMultiByObjectType(Overlaps, NominalEnd, FQuat::Identity, PawnObjects, Shape, Params))
	{
		return BlinkDistance;
	}

	float Dist = BlinkDistance;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		const AActor* PawnActor = Overlap.GetActor();
		if (!PawnActor || PawnActor == Character)
		{
			continue;
		}

		float EnemyRadius = CapsuleRadius;
		if (const ACharacter* EnemyChar = Cast<ACharacter>(PawnActor))
		{
			if (const UCapsuleComponent* EnemyCapsule = EnemyChar->GetCapsuleComponent())
			{
				EnemyRadius = EnemyCapsule->GetScaledCapsuleRadius();
			}
		}

		const float DistAlong = FVector::DotProduct(PawnActor->GetActorLocation() - Start, Dir);
		const float Standoff = DistAlong - (CapsuleRadius + EnemyRadius + EnemyStandoffGap);
		Dist = FMath::Min(Dist, Standoff);
	}

	return FMath::Clamp(Dist, 0.f, BlinkDistance);
}

void UGA_Blink::SetPawnCollisionIgnored(ACharacter* Character, bool bIgnore)
{
	if (!IsValid(Character))
	{
		return;
	}
	UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	if (!Capsule)
	{
		return;
	}

	if (bIgnore)
	{
		if (bPawnIgnored)
		{
			return;
		}
		SavedPawnResponse = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		bPawnIgnored = true;
	}
	else
	{
		if (!bPawnIgnored)
		{
			return;
		}
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, SavedPawnResponse);
		bPawnIgnored = false;
	}
}

void UGA_Blink::OnDashFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}

void UGA_Blink::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		SetPawnCollisionIgnored(Character, false);  // 대시 중이었으면 Pawn 충돌 복원
	}

	if (DashTask)
	{
		DashTask->EndTask();
		DashTask = nullptr;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
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

void UGA_Blink::PlayArrivalMontage(ACharacter* Character) const
{
	UAnimMontage* Montage = ArrivalMontage.LoadSynchronous();
	if (!IsValid(Montage) || !IsValid(Character))
	{
		return;
	}

	if (USkeletalMeshComponent* Mesh = Character->GetMesh())
	{
		if (UAnimInstance* Anim = Mesh->GetAnimInstance())
		{
			Anim->Montage_Play(Montage, ArrivalMontagePlayRate);
		}
	}
}

void UGA_Blink::StopArrivalMontage(ACharacter* Character) const
{
	UAnimMontage* Montage = ArrivalMontage.Get();
	if (!IsValid(Montage) || !IsValid(Character))
	{
		return;
	}

	if (USkeletalMeshComponent* Mesh = Character->GetMesh())
	{
		if (UAnimInstance* Anim = Mesh->GetAnimInstance())
		{
			if (Anim->Montage_IsPlaying(Montage))
			{
				Anim->Montage_Stop(ArrivalMontageBlendOut, Montage);
			}
		}
	}
}
