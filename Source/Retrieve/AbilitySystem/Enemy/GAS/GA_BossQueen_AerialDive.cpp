

#include "GA_BossQueen_AerialDive.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/RetrieveEnemyCharacter.h"
#include "Components/Enemy/EnemyCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DataTable.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "TimerManager.h"

UGA_BossQueen_AerialDive::UGA_BossQueen_AerialDive(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	AbilityTags.AddTag(RetrieveGameplayTags::Ability_Enemy_SpecialAttack);

	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Enemy_SpecialAttack);

	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Dead);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Hit);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Groggy);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Attack);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Boss_PhaseTransition);
	// 특수공격 중첩 활성 차단 (실행 중 owned 태그 보유 → 재활성 차단)
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_SpecialAttack);

	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = RetrieveGameplayTags::GameplayEvent_Enemy_SpecialAttack_AerialDive;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;

	AbilityTriggers.Add(Trigger);
}

void UGA_BossQueen_AerialDive::ActivateAbility(FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	CachedEnemy = Cast<ARetrieveEnemyCharacter>(GetAvatarActorFromActorInfo());

	// [변경] Target은 TWeakObjectPtr<const AActor> → const_cast 불필요
	CachedTarget = TriggerEventData
		? TriggerEventData->Target.Get()
		: nullptr;

	if (CachedEnemy.IsValid() == false || CachedTarget.IsValid() == false)
	{
		FinishAbility(true);
		return;
	}

	if (CommitAbility(Handle, ActorInfo, ActivationInfo) == false)
	{
		FinishAbility(true);
		return;
	}

	CachedCombat = CachedEnemy->FindComponentByClass<UEnemyCombatComponent>();
	CachedMovement = CachedEnemy->GetCharacterMovement();

	if (CachedCombat.IsValid() == false || CachedMovement.IsValid() == false)
	{
		FinishAbility(true);
		return;
	}

	CachedCombat->SetMovementLockedByAttack(true);
	CachedMovement->StopMovementImmediately();
	CachedEnemy->SetAerialMode(true);

	ResolveAerialDiveConfig();

	TakeoffTarget = CachedEnemy->GetActorLocation() + FVector::UpVector * ActiveDiveConfig.TakeoffHeight;

	Stage = EDiveStage::Rising;
	StageElapsed = 0.f;
	TotalElapsed = 0.f;
	DiveTravelled = 0.f;

	ActiveMontage = TriggerEventData
	? const_cast<UAnimMontage*>(
		Cast<UAnimMontage>(
			TriggerEventData->OptionalObject.Get()))
	: nullptr;

	if (IsValid(ActiveMontage)  == false||
		ActiveMontage->IsValidSectionName(TakeoffSection) == false ||
		ActiveMontage->IsValidSectionName(AimSection) == false ||
		ActiveMontage->IsValidSectionName(DiveSection) == false)
	{
		FinishAbility(true);
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		FinishAbility(true);
		return;
	}

	MontageTask =
		UAbilityTask_PlayMontageAndWait::
		CreatePlayMontageAndWaitProxy(
			this,
			NAME_None,
			ActiveMontage,
			1.f,
			TakeoffSection,
			true);

	if (!MontageTask)
	{
		FinishAbility(true);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(
		this,
		&ThisClass::OnMontageFinished);

	MontageTask->OnInterrupted.AddDynamic(
		this,
		&ThisClass::OnMontageInterrupted);

	MontageTask->OnCancelled.AddDynamic(
		this,
		&ThisClass::OnMontageInterrupted);

	MontageTask->ReadyForActivation();

	if (!IsActive())
	{
		return;
	}

	SetMontageSection(TakeoffSection, true);

	LastUpdateTime = World->GetTimeSeconds();

	World->GetTimerManager().SetTimer(
		MotionTimerHandle,
		this,
		&ThisClass::TickMotion,
		0.016f,
		true);
}

void UGA_BossQueen_AerialDive::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (IsEndAbilityValid(Handle, ActorInfo) == false)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (IsValid(World))
	{
		World->GetTimerManager().ClearTimer(MotionTimerHandle);
	}

	if (CachedCombat.IsValid())
	{
		CachedCombat->DeactivateHitbox();
		CachedCombat->SetMovementLockedByAttack(false);
	}

	if (CachedMovement.IsValid())
	{
		CachedMovement->StopMovementImmediately();

		if (CachedMovement->MovementMode == MOVE_Flying && CachedEnemy.IsValid())
		{
			CachedEnemy->SetAerialMode(false);
		}
	}

	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	USkeletalMeshComponent* Mesh = CachedEnemy.IsValid() ? CachedEnemy->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;

	if (ActiveMontage && AnimInstance && AnimInstance->Montage_IsActive(ActiveMontage))
	{
		AnimInstance->Montage_Stop(0.15f, ActiveMontage);
	}
	
	ActiveMontage = nullptr;
	CachedEnemy = nullptr;
	CachedCombat = nullptr;
	CachedMovement = nullptr;
	CachedTarget = nullptr;
	Stage = EDiveStage::None;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UGA_BossQueen_AerialDive::ResolveAerialDiveConfig()
{
	ActiveDiveConfig = FMonsterAerialDiveConfig();
	if (CachedCombat.IsValid() == false)
	{
		return false;
	}

	const UDataTable* Table = CachedCombat->GetPatternTable();
	const FName RowName = CachedCombat->GetActivePatternRowName();
	if (!Table || RowName.IsNone())
	{
		return false;
	}

	const FMonsterPatternRow* Row = Table->FindRow<FMonsterPatternRow>(RowName, TEXT("UGA_BossQueen_AerialDive"));
	if (!Row)
	{
		return false;
	}

	ActiveDiveConfig = Row->AerialDiveConfig;
	return true;
}

void UGA_BossQueen_AerialDive::TickMotion()
{
	UWorld* World = GetWorld();

	const bool bNeedsLiveTarget = Stage == EDiveStage::Rising || Stage == EDiveStage::Aiming;

	if (IsValid(World) == false || CachedEnemy.IsValid() == false || (bNeedsLiveTarget && CachedTarget.IsValid() == false))
	{
		FinishAbility(true);
		return;
	}

	const float CurrentTime = World->GetTimeSeconds();
	const float DeltaTime = FMath::Clamp(CurrentTime - LastUpdateTime, 0.001f, 0.05f);

	LastUpdateTime = CurrentTime;
	StageElapsed += DeltaTime;
	TotalElapsed += DeltaTime;

	if (TotalElapsed >= ActiveDiveConfig.AbilityTimeout)
	{
		FinishAbility(true);
		return;
	}

	switch (Stage)
	{
	case EDiveStage::Rising:
		{
			FaceLocation(CachedTarget->GetActorLocation());

			const FVector ToTarget = TakeoffTarget - CachedEnemy->GetActorLocation();
			const float Distance = ToTarget.Size();

			if (Distance <= ActiveDiveConfig.PositionTolerance)
			{
				BeginAiming();
				return;
			}

			const FVector Step = ToTarget.GetSafeNormal() * FMath::Min(ActiveDiveConfig.RiseSpeed * DeltaTime, Distance);
			float MovedDistance = 0.f;

			if (MoveAvatarSwept(Step, MovedDistance) == false)
			{
				FinishAbility(true);
			}
			break;
		}

	case EDiveStage::Aiming:
		FaceLocation(CachedTarget->GetActorLocation());

		if (StageElapsed >= ActiveDiveConfig.AimDuration)
		{
			BeginDiving();
		}
		break;

	case EDiveStage::Diving:
		{
			const float Remaining = DiveDistance - DiveTravelled;

			if (Remaining <= ActiveDiveConfig.PositionTolerance)
			{
				FinishDive();
				return;
			}

			const FVector Step = DiveDirection * FMath::Min(ActiveDiveConfig.DiveSpeed * DeltaTime, Remaining);
			float MovedDistance = 0.f;
			const bool bNotBlocked = MoveAvatarSwept(Step, MovedDistance);

			DiveTravelled += MovedDistance;

			if (!bNotBlocked)
			{
				FinishDive();
			}
			break;
		}
	}
}

void UGA_BossQueen_AerialDive::BeginAiming()
{
	Stage = EDiveStage::Aiming;
	StageElapsed = 0.f;

	if (CachedMovement.IsValid())
	{
		CachedMovement->StopMovementImmediately();
	}
	SetMontageSection(AimSection, true);
}

void UGA_BossQueen_AerialDive::BeginDiving()
{
	if (CachedEnemy.IsValid() == false || CachedTarget.IsValid() == false)
	{
		FinishAbility(true);
		return;
	}

	const FVector CurrentLocation = CachedEnemy->GetActorLocation();
	const FVector TargetLocation = CachedTarget->GetActorLocation() + ActiveDiveConfig.TargetOffset;

	FVector ApproachDirection = TargetLocation - CurrentLocation;

	// 정지 거리는 수평 기준
	ApproachDirection.Z = 0.f;

	if (ApproachDirection.IsNearlyZero())
	{
		FinishAbility(true);
		return;
	}

	ApproachDirection.Normalize();

	LockedTargetLocation = TargetLocation - ApproachDirection * ActiveDiveConfig.DiveStopDistance;
	DiveDirection =	(LockedTargetLocation - CurrentLocation).GetSafeNormal();

	if (DiveDirection.IsNearlyZero())
	{
		FinishAbility(true);
		return;
	}

	DiveDistance = FVector::Distance(CurrentLocation, LockedTargetLocation);

	DiveTravelled = 0.f;
	Stage = EDiveStage::Diving;
	StageElapsed = 0.f;

	FaceLocation(LockedTargetLocation);
	SetMontageSection(DiveSection, true);

	if (CachedCombat.IsValid())
	{
		CachedCombat->ActivateHitbox();
	}
}

void UGA_BossQueen_AerialDive::FinishDive()
{
	if (!IsActive())
	{
		return;
	}

	if (CachedEnemy.IsValid())
	{
		// Flying을 해제하고 Falling으로 전환
		CachedEnemy->SetAerialMode(false);
	}

	FinishAbility(false);
}

void UGA_BossQueen_AerialDive::FinishAbility(bool bWasCancelled)
{
	if (IsActive() == false)
	{
		return;
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
}

void UGA_BossQueen_AerialDive::FaceLocation(const FVector& Location)
{
	if (CachedEnemy.IsValid() == false)
	{
		return;
	}

	FVector Direction = Location - CachedEnemy->GetActorLocation();

	Direction.Z = 0.f;

	if (Direction.IsNearlyZero() == false)
	{
		CachedEnemy->SetActorRotation(Direction.Rotation());
	}
}

void UGA_BossQueen_AerialDive::SetMontageSection(FName SectionName, bool bLoop)
{
if (IsValid(ActiveMontage) == false|| SectionName.IsNone() || ActiveMontage->IsValidSectionName(SectionName) == false || CachedEnemy.IsValid() == false)
	{
		return;
	}

	USkeletalMeshComponent* Mesh = CachedEnemy->GetMesh();
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;

	if (IsValid(AnimInstance) == false || AnimInstance->Montage_IsActive(ActiveMontage) == false)
	{
		return;
	}

	AnimInstance->Montage_SetNextSection(SectionName, bLoop ? SectionName : NAME_None, ActiveMontage);
	AnimInstance->Montage_JumpToSection(SectionName, ActiveMontage);
}

bool UGA_BossQueen_AerialDive::MoveAvatarSwept(const FVector& Delta, float& OutMovedDistance)
{
	OutMovedDistance = 0.f;

	if (CachedEnemy.IsValid() == false)
	{
		return false;
	}

	const FVector Before = CachedEnemy->GetActorLocation();

	FHitResult Hit;

	CachedEnemy->AddActorWorldOffset(
		Delta,
		true,
		&Hit,
		ETeleportType::None);

	OutMovedDistance = FVector::Distance(Before, CachedEnemy->GetActorLocation());

	return !Hit.IsValidBlockingHit();
}

void UGA_BossQueen_AerialDive::OnMontageFinished()
{
	MontageTask = nullptr;

	// 모든 섹션이 반복되므로 정상적으로 먼저 끝날 상황은 없음
	if (IsActive())
	{
		FinishAbility(true);
	}
}

void UGA_BossQueen_AerialDive::OnMontageInterrupted()
{
	FinishAbility(true);
}
