#include "AbilitySystem/Enemy/GAS/GA_Enemy_DiveImpact.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Components/DecalComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/Enemy/EnemyCombatComponent.h"
#include "Engine/World.h"
#include "NiagaraFunctionLibrary.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Character/RetrieveEnemyCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"

UGA_Enemy_DiveImpact::UGA_Enemy_DiveImpact(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilityTags.AddTag(RetrieveGameplayTags::Ability_Enemy_SpecialAttack);
	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Enemy_SpecialAttack);

	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Dead);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Hit);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Groggy);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Attack);

	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = RetrieveGameplayTags::GameplayEvent_Enemy_SpecialAttack_AerialDive;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);
}

void UGA_Enemy_DiveImpact::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	CachedEnemy = Cast<ARetrieveEnemyCharacter>(GetAvatarActorFromActorInfo());
	CachedTarget = ResolveTargetActor(TriggerEventData);
	ActiveMontage = ResolveAttackMontage(TriggerEventData);

	if (!CachedEnemy.IsValid() || !CachedTarget.IsValid() || !ActiveMontage)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	SetDiveCapsulePawnOverlap();

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		ActiveMontage,
		GetAttackMontagePlayRate(MontagePlayRate),
		NAME_None,
		true);
	if (!MontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageInterrupted);
	MontageTask->ReadyForActivation();

	CachedEnemy->SetAerialMode(true);
	BeginTakeOff();
}

void UGA_Enemy_DiveImpact::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MotionTimerHandle);
	}

	FadeWarningDecal();

	if (CachedEnemy.IsValid())
	{
		CachedEnemy->SetAerialMode(false);
	}

	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	ActiveMontage = nullptr;
	CachedEnemy = nullptr;
	CachedTarget = nullptr;
	Phase = EEnemyDiveImpactPhase::None;
	PhaseElapsed = 0.f;
	LastUpdateTime = 0.f;
	bWarningSpawned = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_Enemy_DiveImpact::OnMontageCompleted()
{
	MontageTask = nullptr;
	FinishAbility(false);
}

void UGA_Enemy_DiveImpact::OnMontageInterrupted()
{
	FinishAbility(true);
}

UAnimMontage* UGA_Enemy_DiveImpact::ResolveAttackMontage(const FGameplayEventData* TriggerEventData) const
{
	return TriggerEventData
		? const_cast<UAnimMontage*>(Cast<UAnimMontage>(TriggerEventData->OptionalObject.Get()))
		: nullptr;
}

const AActor* UGA_Enemy_DiveImpact::ResolveTargetActor(const FGameplayEventData* TriggerEventData) const
{
	return TriggerEventData ? TriggerEventData->Target.Get() : nullptr;
}

void UGA_Enemy_DiveImpact::BeginTakeOff()
{
	UWorld* World = GetWorld();
	if (!World || !CachedEnemy.IsValid())
	{
		FinishAbility(true);
		return;
	}

	Phase = EEnemyDiveImpactPhase::TakeOff;
	PhaseElapsed = 0.f;
	bWarningSpawned = false;
	PhaseStartLocation = CachedEnemy->GetActorLocation();
	HoverLocation = PhaseStartLocation + FVector::UpVector * TakeOffHeight;
	LastUpdateTime = World->GetTimeSeconds();

	World->GetTimerManager().SetTimer(
		MotionTimerHandle,
		this,
		&ThisClass::TickDiveMotion,
		FMath::Max(0.001f, MotionTickInterval),
		true);
}

void UGA_Enemy_DiveImpact::BeginAirFollow()
{
	Phase = EEnemyDiveImpactPhase::AirFollow;
	PhaseElapsed = 0.f;
	PhaseStartLocation = CachedEnemy.IsValid() ? CachedEnemy->GetActorLocation() : FVector::ZeroVector;
}

void UGA_Enemy_DiveImpact::BeginDive()
{
	if (!CachedEnemy.IsValid())
	{
		FinishAbility(true);
		return;
	}

	FVector GroundLocation = CachedEnemy->GetActorLocation();
	if (CachedTarget.IsValid())
	{
		GroundLocation = CachedTarget->GetActorLocation();
	}

	if (!bWarningSpawned)
	{
		ResolveGroundLocation(GroundLocation, DiveTargetLocation);
		SpawnWarningDecal();
	}

	Phase = EEnemyDiveImpactPhase::Dive;
	PhaseElapsed = 0.f;
	PhaseStartLocation = CachedEnemy->GetActorLocation();
	FaceLocation(DiveTargetLocation);
}

void UGA_Enemy_DiveImpact::TickDiveMotion()
{
	UWorld* World = GetWorld();
	if (!World || !CachedEnemy.IsValid())
	{
		FinishAbility(true);
		return;
	}

	const float CurrentTime = World->GetTimeSeconds();
	const float DeltaTime = FMath::Clamp(CurrentTime - LastUpdateTime, 0.001f, 0.05f);
	LastUpdateTime = CurrentTime;
	PhaseElapsed += DeltaTime;

	switch (Phase)
	{
	case EEnemyDiveImpactPhase::TakeOff:
		{
			const float Alpha = FMath::Clamp(PhaseElapsed / FMath::Max(0.01f, TakeOffDuration), 0.f, 1.f);
			CachedEnemy->SetActorLocation(FMath::Lerp(PhaseStartLocation, HoverLocation, Alpha));
			if (CachedTarget.IsValid())
			{
				FaceLocation(CachedTarget->GetActorLocation());
			}

			if (Alpha >= 1.f)
			{
				BeginAirFollow();
			}
			break;
		}
	case EEnemyDiveImpactPhase::AirFollow:
		{
			if (!CachedTarget.IsValid())
			{
				FinishAbility(true);
				return;
			}

			const FVector CurrentLocation = CachedEnemy->GetActorLocation();
			const FVector TargetLocation = CachedTarget->GetActorLocation();
			const FVector DesiredLocation(TargetLocation.X, TargetLocation.Y, HoverLocation.Z);
			
			const FVector CurrentXY(CurrentLocation.X, CurrentLocation.Y, 0.f);
			const FVector DesiredXY(DesiredLocation.X, DesiredLocation.Y, 0.f);
			const float DistanceXY = FVector::Dist(CurrentXY, DesiredXY);

			if (DistanceXY > AirFollowLocationTolerance)
			{
				const FVector NewLocation = FMath::VInterpTo(
					CurrentLocation,
					DesiredLocation,
					DeltaTime,
					AirFollowInterpSpeed);

				CachedEnemy->SetActorLocation(NewLocation);
			}
			
			FaceLocation(TargetLocation);

			const float RemainingTime = AirFollowDuration - PhaseElapsed;
			if (!bWarningSpawned && RemainingTime <= WarningLeadTime)
			{
				FVector GroundLocation;
				if (ResolveGroundLocation(TargetLocation, GroundLocation))
				{
					DiveTargetLocation = GroundLocation;
					SpawnWarningDecal();
				}
			}

			if (PhaseElapsed >= AirFollowDuration)
			{
				BeginDive();
			}
			break;
		}
	case EEnemyDiveImpactPhase::Dive:
		{
			const float Alpha = FMath::Clamp(PhaseElapsed / FMath::Max(0.01f, DiveDuration), 0.f, 1.f);
			CachedEnemy->SetActorLocation(FMath::Lerp(PhaseStartLocation, DiveTargetLocation, Alpha), true);
			if (Alpha >= 1.f)
			{
				FinishDive();
			}
			break;
		}
	default:
		break;
	}
}

void UGA_Enemy_DiveImpact::FinishDive()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MotionTimerHandle);
	}

	Phase = EEnemyDiveImpactPhase::None;

	if (CachedEnemy.IsValid())
	{
		CachedEnemy->SetAerialMode(false);
	}
	
	ApplyImpact();
	
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->CurrentMontageJumpToSection(ImpactSectionName);
	}
}

void UGA_Enemy_DiveImpact::FinishAbility(bool bWasCancelled)
{
	if (!IsActive())
	{
		return;
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
}

void UGA_Enemy_DiveImpact::SpawnWarningDecal()
{
	if (bWarningSpawned || !WarningDecalMaterial)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(
		World,
		WarningDecalMaterial,
		FVector(WarningDecalDepth, WarningDecalRadius, WarningDecalRadius),
		DiveTargetLocation,
		FRotator(-90.f, 0.f, 0.f));

	if (Decal)
	{
		WarningDecal = Decal;
	}

	bWarningSpawned = true;
}

bool UGA_Enemy_DiveImpact::ResolveGroundLocation(const FVector& SourceLocation, FVector& OutGroundLocation) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		OutGroundLocation = SourceLocation;
		return false;
	}

	const FVector TraceStart = SourceLocation + FVector::UpVector * GroundTraceUpDistance;
	const FVector TraceEnd = SourceLocation - FVector::UpVector * GroundTraceDownDistance;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyDiveImpactGroundTrace), false);
	if (CachedEnemy.IsValid())
	{
		QueryParams.AddIgnoredActor(CachedEnemy.Get());
	}

	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
	{
		OutGroundLocation = Hit.ImpactPoint + FVector::UpVector * GroundOffset;
		return true;
	}

	OutGroundLocation = SourceLocation;
	OutGroundLocation.Z += GroundOffset;
	return false;
}

void UGA_Enemy_DiveImpact::FaceLocation(const FVector& Location) const
{
	if (!CachedEnemy.IsValid())
	{
		return;
	}

	FVector Direction = Location - CachedEnemy->GetActorLocation();
	Direction.Z = 0.f;
	if (!Direction.IsNearlyZero())
	{
		CachedEnemy->SetActorRotation(Direction.Rotation());
	}
}

void UGA_Enemy_DiveImpact::ApplyImpact()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(CollisionRestoreTimerHandle, 
			this, &ThisClass::RestoreDiveCapsulePawnResponse, RestoreTime);
	}
	
	if (!CachedEnemy.IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (ImpactVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			ImpactVFX,
			DiveTargetLocation,
			FRotator::ZeroRotator,
			ImpactVFXScale);
	}

	if (UEnemyCombatComponent* Combat = GetEnemyCombatComponent())
	{
		Combat->ActivateHitbox();
		Combat->DeactivateHitbox();
	}
}

void UGA_Enemy_DiveImpact::FadeWarningDecal()
{
	if (UDecalComponent* Decal = WarningDecal.Get())
	{
		Decal->SetFadeOut(0.f, WarningDecalFadeOutDuration, true);
	}
	WarningDecal = nullptr;
}

void UGA_Enemy_DiveImpact::SetDiveCapsulePawnOverlap()
{
	if (!CachedEnemy.IsValid() || bDidOverrideCapsulePawnResponse)
	{
		return;
	}

	UCapsuleComponent* Capsule = CachedEnemy->GetCapsuleComponent();
	if (!Capsule)
	{
		return;
	}

	CachedCapsulePawnResponse = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
	Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	bDidOverrideCapsulePawnResponse = true;
}

void UGA_Enemy_DiveImpact::RestoreDiveCapsulePawnResponse()
{
	if (!bDidOverrideCapsulePawnResponse)
	{
		return;
	}

	if (CachedEnemy.IsValid())
	{
		if (UCapsuleComponent* Capsule = CachedEnemy->GetCapsuleComponent())
		{
			Capsule->SetCollisionResponseToChannel(ECC_Pawn, CachedCapsulePawnResponse);
		}
	}

	bDidOverrideCapsulePawnResponse = false;
}
