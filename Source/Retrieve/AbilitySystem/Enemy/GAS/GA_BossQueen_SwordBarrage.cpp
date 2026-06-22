

#include "GA_BossQueen_SwordBarrage.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystem/Enemy/QueenSwordProjectile.h"
#include "Animation/AnimMontage.h"
#include "Components/SceneComponent.h"
#include "Components/Enemy/EnemyCombatComponent.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "TimerManager.h"

UGA_BossQueen_SwordBarrage::UGA_BossQueen_SwordBarrage(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	AbilityTags.AddTag(RetrieveGameplayTags::Ability_Enemy_SpecialAttack);

	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Enemy_SpecialAttack);

	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Dead);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Hit);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Groggy);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Attack);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Boss_PhaseTransition);

	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = RetrieveGameplayTags::GameplayEvent_Enemy_SpecialAttack_Projectile;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;

	AbilityTriggers.Add(TriggerData);
}

void UGA_BossQueen_SwordBarrage::ActivateAbility(
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

	CachedTargetActor = TriggerEventData
		? const_cast<AActor*>(TriggerEventData->Target.Get())
		: nullptr;

	if (ProjectileClass == nullptr || IsValid(CachedTargetActor) == false)
	{
		FinishAbility(true);
		return;
	}

	ResolveBarrageConfig();

	bMontageFinished = false;
	bLaunchFinished = false;
	PreparedSwords.Reset();

	UAnimMontage* Montage = ResolveMontage(TriggerEventData);

	if (IsValid(Montage))
	{
		MontageTask =
			UAbilityTask_PlayMontageAndWait::
			CreatePlayMontageAndWaitProxy(
				this, NAME_None, Montage);

		if (!MontageTask)
		{
			FinishAbility(true);
			return;
		}

		MontageTask->OnCompleted.AddDynamic(
			this, &ThisClass::OnMontageCompleted);
		MontageTask->OnBlendOut.AddDynamic(
			this, &ThisClass::OnMontageCompleted);
		MontageTask->OnInterrupted.AddDynamic(
			this, &ThisClass::OnMontageInterrupted);
		MontageTask->OnCancelled.AddDynamic(
			this, &ThisClass::OnMontageInterrupted);

		MontageTask->ReadyForActivation();
	}
	else
	{
		bMontageFinished = true;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		FinishAbility(true);
		return;
	}

	if (ActiveConfig.SummonDelay <= 0.f)
	{
		HandleSummon();
	}
	else
	{
		World->GetTimerManager().SetTimer(
			SummonTimerHandle,
			this,
			&ThisClass::HandleSummon,
			ActiveConfig.SummonDelay,
			false);
	}
}

void UGA_BossQueen_SwordBarrage::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (!IsEndAbilityValid(Handle, ActorInfo))
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SummonTimerHandle);
		World->GetTimerManager().ClearTimer(LaunchTimerHandle);
	}

	CleanupPreparedSwords();

	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	CachedTargetActor = nullptr;
	bMontageFinished = false;
	bLaunchFinished = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	
}

bool UGA_BossQueen_SwordBarrage::ResolveBarrageConfig()
{
	ActiveConfig = FMonsterSwordBarrageConfig();   // 기본값 = 폴백

	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const UEnemyCombatComponent* Combat =
		Avatar ? Avatar->FindComponentByClass<UEnemyCombatComponent>() : nullptr;
	if (!Combat)
	{
		return false;
	}

	const UDataTable* Table = Combat->GetPatternTable();
	const FName RowName = Combat->GetActivePatternRowName();
	if (!Table || RowName.IsNone())
	{
		return false;
	}

	const FMonsterPatternRow* Row = Table->FindRow<FMonsterPatternRow>(RowName, TEXT("UGA_BossQueen_SwordBarrage"));
	if (!Row)
	{
		return false;
	}

	ActiveConfig = Row->SwordBarrageConfig;
	return true;
}

void UGA_BossQueen_SwordBarrage::HandleSummon()
{
	if (IsActive() == false || HasAuthority(&GetCurrentActivationInfoRef()) == false)
	{
		return;
	}

	SpawnSwordArc();

	if (PreparedSwords.IsEmpty())
	{
		FinishAbility(true);
		return;
	}

	if (ActiveConfig.LaunchDelay <= 0.f)
	{
		LaunchPreparedSwords();
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(
		LaunchTimerHandle,
		this,
		&ThisClass::LaunchPreparedSwords,
		ActiveConfig.LaunchDelay,
		false);
}

void UGA_BossQueen_SwordBarrage::SpawnSwordArc()
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();

	if (IsValid(AvatarActor) == false || World == nullptr|| IsValid(ProjectileClass) == false || IsValid(CachedTargetActor) == false)
	{
		return;
	}

	USceneComponent* FollowComponent =
		AvatarActor->GetRootComponent();

	if (IsValid(FollowComponent) == false)
	{
		return;
	}

	const FVector Up = AvatarActor->GetActorUpVector();
	const FVector Right = AvatarActor->GetActorRightVector();
	const FVector Forward = AvatarActor->GetActorForwardVector();

	const FVector Center =
		AvatarActor->GetActorLocation()
		+ Forward * ActiveConfig.ForwardOffset
		+ Up * ActiveConfig.HeightOffset;

	const int32 Count = FMath::Max(1, ActiveConfig.SwordCount);
	const float HalfAngle = ActiveConfig.ArcAngleDegrees * 0.5f;
	const float StartAngle = 90.f - HalfAngle;
	const float EndAngle = 90.f + HalfAngle;
	const FVector AimLocation = ResolveTargetLocation();

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Alpha = Count > 1
			? static_cast<float>(Index) /
				static_cast<float>(Count - 1)
			: 0.5f;

		const float AngleDegrees =
			FMath::Lerp(StartAngle, EndAngle, Alpha);

		const float AngleRadians =
			FMath::DegreesToRadians(AngleDegrees);

		const FVector Offset =
			Right * FMath::Cos(AngleRadians) * ActiveConfig.ArcRadius
			+ Up * FMath::Sin(AngleRadians) * ActiveConfig.ArcRadius;

		const FVector SpawnLocation = Center + Offset;
		const FVector Direction =
			(AimLocation - SpawnLocation).GetSafeNormal();

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = AvatarActor;
		SpawnParams.Instigator = Cast<APawn>(AvatarActor);
		SpawnParams.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::
				AdjustIfPossibleButAlwaysSpawn;

		AQueenSwordProjectile* Sword =
			World->SpawnActor<AQueenSwordProjectile>(
				ProjectileClass,
				SpawnLocation,
				Direction.Rotation(),
				SpawnParams);

		if (Sword == nullptr)
		{
			continue;
		}

		Sword->PrepareProjectile(FollowComponent);
		PreparedSwords.Add(Sword);
	}
}

void UGA_BossQueen_SwordBarrage::LaunchPreparedSwords()
{
	if (!IsActive())
	{
		return;
	}

	if (IsValid(CachedTargetActor) == false)
	{
		FinishAbility(true);
		return;
	}

	for (AQueenSwordProjectile* Sword : PreparedSwords)
	{
		if (IsValid(Sword) == false)
		{
			continue;
		}

		if (!Sword->FireAtTarget(CachedTargetActor, ActiveConfig.ProjectileSpeed, ActiveConfig.ProjectileLifetime, ActiveConfig.TargetOffset))
		{
			Sword->Destroy();
		}
	}

	PreparedSwords.Reset();
	bLaunchFinished = true;
	TryFinishAbility();
}

void UGA_BossQueen_SwordBarrage::CleanupPreparedSwords()
{
	for (AQueenSwordProjectile* Sword : PreparedSwords)
	{
		if (IsValid(Sword))
		{
			Sword->Destroy();
		}
	}

	PreparedSwords.Reset();
}

void UGA_BossQueen_SwordBarrage::TryFinishAbility()
{
	if (bMontageFinished && bLaunchFinished)
	{
		FinishAbility(false);
	}
}

void UGA_BossQueen_SwordBarrage::FinishAbility(bool bWasCancelled)
{
	if (!IsActive())
	{
		return;
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
}

UAnimMontage* UGA_BossQueen_SwordBarrage::ResolveMontage(const FGameplayEventData* TriggerEventData) const
{
	if (!TriggerEventData)
	{
		return nullptr;
	}

	return const_cast<UAnimMontage*>(Cast<UAnimMontage>(TriggerEventData->OptionalObject.Get()));
}

FVector UGA_BossQueen_SwordBarrage::ResolveTargetLocation() const
{
	return IsValid(CachedTargetActor)
		? CachedTargetActor->GetActorLocation() + ActiveConfig.TargetOffset
		: FVector::ZeroVector;
}

void UGA_BossQueen_SwordBarrage::OnMontageCompleted()
{
	if (bMontageFinished)
	{
		return;
	}

	bMontageFinished = true;
	TryFinishAbility();
}

void UGA_BossQueen_SwordBarrage::OnMontageInterrupted()
{
	FinishAbility(true);
}
