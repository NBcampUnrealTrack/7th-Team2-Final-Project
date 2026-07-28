#include "Combat/RetrieveKnockbackLibrary.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "TimerManager.h"

void URetrieveKnockbackLibrary::DoKnockback(ACharacter* Target, const FVector& NormalizedDir, float Strength, float UpwardStrength, float Duration)
{
	if (!IsValid(Target))
	{
		return;
	}

	UCharacterMovementComponent* MoveComp = Target->GetCharacterMovement();
	if (!MoveComp)
	{
		return;
	}

	// 넉백 면역(가디언·보스 등): 면역 태그 보유 시 스킵.
	if (const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target))
	{
		if (ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Immune_Knockback))
		{
			return;
		}
	}

	const FVector Velocity = NormalizedDir * Strength + FVector(0.f, 0.f, UpwardStrength);
	if (Velocity.IsNearlyZero() || Duration <= 0.f)
	{
		return;
	}

	// LaunchCharacter/AddImpulse는 StopMovementImmediately()의 ClearAccumulatedForces()에 지워져
	// 적 피격 시 movement 정지/락/AI 제어를 못 이긴다. Root Motion Source는 별도 채널이라 이를 우회한다.
	static const FName KnockbackSourceName(TEXT("RetrieveKnockback"));
	MoveComp->RemoveRootMotionSource(KnockbackSourceName); // 연타 시 중첩 방지

	TSharedPtr<FRootMotionSource_ConstantForce> Source = MakeShared<FRootMotionSource_ConstantForce>();
	Source->InstanceName = KnockbackSourceName;
	Source->AccumulateMode = ERootMotionAccumulateMode::Override;
	Source->Priority = 6;
	Source->Force = Velocity;
	Source->Duration = Duration;
	Source->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::ClampVelocity;
	Source->FinishVelocityParams.ClampVelocity = 0.f;
	MoveComp->ApplyRootMotionSource(Source);
}

void URetrieveKnockbackLibrary::DoLaunch(ACharacter* Target, const FVector& NormalizedDir, float Strength, float UpwardStrength, bool bOverrideXY, bool bOverrideZ)
{
	if (!IsValid(Target))
	{
		return;
	}

	const FVector LaunchVelocity = NormalizedDir * Strength + FVector(0.f, 0.f, UpwardStrength);
	if (LaunchVelocity.IsNearlyZero())
	{
		return;
	}

	Target->LaunchCharacter(LaunchVelocity, bOverrideXY, bOverrideZ);
}

void URetrieveKnockbackLibrary::ApplyKnockbackFromSource(ACharacter* Target, const FVector& SourceLocation, const FRetrieveKnockbackParams& Params)
{
	if (!IsValid(Target))
	{
		return;
	}

	const FVector Direction = (Target->GetActorLocation() - SourceLocation).GetSafeNormal();
	DoKnockback(Target, Direction, Params.Strength, Params.UpwardStrength, Params.Duration);
}

void URetrieveKnockbackLibrary::ApplyPlanarKnockbackFromActor(ACharacter* Target, AActor* SourceActor, const FRetrieveKnockbackParams& Params)
{
	if (!IsValid(Target) || !IsValid(SourceActor) || Target == SourceActor || Params.Duration <= 0.f)
	{
		return;
	}

	if (const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target))
	{
		if (ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Immune_Knockback))
		{
			return;
		}
	}

	const FVector TargetLocation = Target->GetActorLocation();
	const FVector SourceLocation = SourceActor->GetActorLocation();
	FVector Direction = Target->GetActorLocation() - SourceActor->GetActorLocation();
	Direction.Z = 0.f;
	const FVector RawPlanarDelta = Direction;
	const bool bUsedFallbackDirection = Direction.IsNearlyZero();
	if (Direction.IsNearlyZero())
	{
		Direction = SourceActor->GetActorForwardVector();
		Direction.Z = 0.f;
	}

	Direction = Direction.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return;
	}

	Target->MoveIgnoreActorAdd(SourceActor);

	if (UWorld* World = Target->GetWorld())
	{
		TWeakObjectPtr<ACharacter> WeakTarget(Target);
		TWeakObjectPtr<AActor> WeakSourceActor(SourceActor);

		FTimerDelegate RestoreCollisionDelegate;
		RestoreCollisionDelegate.BindLambda([WeakTarget, WeakSourceActor]()
		{
			if (WeakTarget.IsValid() && WeakSourceActor.IsValid())
			{
				WeakTarget->MoveIgnoreActorRemove(WeakSourceActor.Get());
			}
		});

		FTimerHandle RestoreCollisionHandle;
		World->GetTimerManager().SetTimer(RestoreCollisionHandle, RestoreCollisionDelegate, Params.Duration + 0.05f, false);
	}

	if (Params.bCancelTargetActions)
	{
		if (UAbilitySystemComponent* TargetASC =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target))
		{
			FGameplayTagContainer TagsToCancel;
			TagsToCancel.AddTag(RetrieveGameplayTags::Ability_Type_Attack);
			TagsToCancel.AddTag(RetrieveGameplayTags::Ability_Player_Guard);
			
			TargetASC->CancelAbilities(&TagsToCancel, nullptr, nullptr);

			TargetASC->AddLooseGameplayTag(RetrieveGameplayTags::State_Player_ForcedKnockback);

			if (UWorld* World = Target->GetWorld())
			{
				TWeakObjectPtr<UAbilitySystemComponent> WeakTargetASC(TargetASC);

				FTimerDelegate RemoveForcedKnockbackDelegate;
				RemoveForcedKnockbackDelegate.BindLambda([WeakTargetASC]()
				{
					if (WeakTargetASC.IsValid())
					{
						WeakTargetASC->RemoveLooseGameplayTag(RetrieveGameplayTags::State_Player_ForcedKnockback);
					}
				});

				FTimerHandle RemoveForcedKnockbackHandle;
				World->GetTimerManager().SetTimer(RemoveForcedKnockbackHandle, RemoveForcedKnockbackDelegate, Params.Duration + 0.05f, false);
			}
		}
	}
	DoKnockback(Target, Direction, Params.Strength, Params.UpwardStrength, Params.Duration);
}

void URetrieveKnockbackLibrary::ApplyKnockbackInDirection(ACharacter* Target, const FVector& WorldDirection, const FRetrieveKnockbackParams& Params)
{
	DoKnockback(Target, WorldDirection.GetSafeNormal(), Params.Strength, Params.UpwardStrength, Params.Duration);
}

int32 URetrieveKnockbackLibrary::ApplyRadialKnockback(const UObject* WorldContextObject, const FVector& Center, float Radius,
	const FRetrieveKnockbackParams& Params, const TArray<AActor*>& IgnoreActors)
{
	if (!WorldContextObject || Radius <= 0.f)
	{
		return 0;
	}

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return 0;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RetrieveRadialKnockback), false);
	QueryParams.AddIgnoredActors(IgnoreActors);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, ObjectQueryParams, FCollisionShape::MakeSphere(Radius), QueryParams);

	TArray<ACharacter*> Targets;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (ACharacter* Character = Cast<ACharacter>(Overlap.GetActor()))
		{
			Targets.AddUnique(Character);
		}
	}

	ApplyRadialKnockbackToTargets(Center, Radius, Targets, Params);
	return Targets.Num();
}

void URetrieveKnockbackLibrary::ApplyRadialKnockbackToTargets(const FVector& Center, float Radius, const TArray<ACharacter*>& Targets, const FRetrieveKnockbackParams& Params)
{
	const bool bScale = Params.bScaleByDistance && Radius > KINDA_SMALL_NUMBER;

	for (ACharacter* Target : Targets)
	{
		if (!IsValid(Target))
		{
			continue;
		}

		// 수평 방향으로 밀친다(상향은 UpwardStrength로 별도 합성).
		FVector Delta = Target->GetActorLocation() - Center;
		Delta.Z = 0.f;
		const FVector Direction = Delta.GetSafeNormal();

		float StrengthScale = 1.f;
		if (bScale)
		{
			const float DistRatio = FMath::Clamp(Delta.Size() / Radius, 0.f, 1.f);
			StrengthScale = FMath::Lerp(1.f, Params.EdgeStrengthRatio, DistRatio);
		}

		DoKnockback(Target, Direction, Params.Strength * StrengthScale, Params.UpwardStrength * StrengthScale, Params.Duration);
	}
}

void URetrieveKnockbackLibrary::LaunchSelf(ACharacter* Character, const FVector& WorldDirection, float Speed, bool bOverrideXY, bool bOverrideZ)
{
	// 자기발사는 순수 추진(상향 없음). 수직은 기본적으로 보존(bOverrideZ=false).
	DoLaunch(Character, WorldDirection.GetSafeNormal(), Speed, 0.f, bOverrideXY, bOverrideZ);
}
