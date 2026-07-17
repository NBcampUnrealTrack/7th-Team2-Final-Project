#include "AbilitySystem/Enemy/GAS/GA_EnemyBowShot.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Enemy/EnemyProjectile.h"
#include "Animation/AnimMontage.h"
#include "Character/Cosmetics/RetrieveBowMeshAnimInstance.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "TimerManager.h"

UGA_EnemyBowShot::UGA_EnemyBowShot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilityTriggers.Reset();

	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = RetrieveGameplayTags::GameplayEvent_Enemy_SpecialAttack_Projectile;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);

	FallbackProjectileSpawnDelay = 0.f;
	FallbackProjectileSpeed = 2500.f;
}

void UGA_EnemyBowShot::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PhaseTimerHandle);
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->CurrentMontageStop(0.1f);
	}

	StopPhaseMontageTask();

	if (URetrieveBowMeshAnimInstance* BowAnim = CachedBowMeshAnimInstance.Get())
	{
		BowAnim->Montage_Stop(0.1f);
	}

	ShotStage = EEnemyBowShotStage::None;
	CachedBowMeshAnimInstance.Reset();
	CachedBowMeshComponent.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

const UAnimMontage* UGA_EnemyBowShot::ResolveMontage(const FGameplayEventData* TriggerEventData) const
{
	return CharacterShotMontages.Resolve(EBowShotPhase::DrawnStart, false);
}

void UGA_EnemyBowShot::OnSpecialAttackActivated()
{
	ShotStage = EEnemyBowShotStage::DrawIntro;
	CacheBowMesh();
	FaceTarget();

	const bool bHasDrawIntro = CharacterShotMontages.Resolve(EBowShotPhase::DrawnStart, false) != nullptr;
	if (bHasDrawIntro)
	{
		PlayBowMeshPhase(EBowShotPhase::DrawnStart);
	}
	else
	{
		StartDrawHold();
	}
}

void UGA_EnemyBowShot::OnSpecialAttackEnded()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PhaseTimerHandle);
	}
}

void UGA_EnemyBowShot::OnBeforeProjectileSpawn()
{
	FaceTarget();
}

void UGA_EnemyBowShot::OnProjectileSpawned(AEnemyProjectile* Projectile, AActor* AvatarActor)
{
	Super::OnProjectileSpawned(Projectile, AvatarActor);

	const FMonsterProjectilePatternConfig& Config = GetActiveProjectileConfig();
	if (IsValid(Projectile) && Config.bUseGravity)
	{
		Projectile->SetBallisticGravityScale(Config.ProjectileGravityScale);
	}
}

USkeletalMeshComponent* UGA_EnemyBowShot::ResolveProjectileSpawnMesh(AActor* AvatarActor) const
{
	if (USkeletalMeshComponent* BowMesh = CachedBowMeshComponent.Get())
	{
		if (SpawnSocketName.IsNone() || BowMesh->DoesSocketExist(SpawnSocketName))
		{
			return BowMesh;
		}
	}

	return Super::ResolveProjectileSpawnMesh(AvatarActor);
}

FVector UGA_EnemyBowShot::ResolveAimedProjectileDirection(
	const FVector& SpawnLocation,
	AActor* TargetActor) const
{
	if (!IsValid(TargetActor))
	{
		return Super::ResolveAimedProjectileDirection(SpawnLocation, TargetActor);
	}

	FVector AimLocation = TargetActor->GetActorLocation();
	if (const UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(TargetActor->GetRootComponent()))
	{
		AimLocation = RootPrimitive->Bounds.Origin;
	}
	AimLocation += TargetAimOffset;

	const FVector DirectDirection = (AimLocation - SpawnLocation).GetSafeNormal();
	const FMonsterProjectilePatternConfig& Config = GetActiveProjectileConfig();
	if (!bUseBallisticArc
		|| !Config.bUseGravity
		|| Config.ProjectileGravityScale <= 0.f
		|| Config.ProjectileSpeed <= 0.f)
	{
		return DirectDirection;
	}

	const UWorld* World = GetWorld();
	const float Gravity = World
		? FMath::Abs(World->GetGravityZ()) * Config.ProjectileGravityScale
		: 0.f;
	if (Gravity <= KINDA_SMALL_NUMBER)
	{
		return DirectDirection;
	}

	const FVector Delta = AimLocation - SpawnLocation;
	const FVector HorizontalDelta(Delta.X, Delta.Y, 0.f);
	const float HorizontalDistance = HorizontalDelta.Size();
	if (HorizontalDistance <= KINDA_SMALL_NUMBER)
	{
		return DirectDirection;
	}

	const float SpeedSquared = FMath::Square(Config.ProjectileSpeed);
	const float Discriminant = FMath::Square(SpeedSquared)
		- Gravity * (Gravity * FMath::Square(HorizontalDistance) + 2.f * Delta.Z * SpeedSquared);
	if (Discriminant < 0.f)
	{
		return DirectDirection;
	}

	const float TanTheta = (SpeedSquared - FMath::Sqrt(Discriminant))
		/ (Gravity * HorizontalDistance);
	const float CosTheta = 1.f / FMath::Sqrt(1.f + FMath::Square(TanTheta));
	const float SinTheta = TanTheta * CosTheta;
	const FVector LaunchVelocity = HorizontalDelta.GetSafeNormal() * (Config.ProjectileSpeed * CosTheta)
		+ FVector::UpVector * (Config.ProjectileSpeed * SinTheta);

	return LaunchVelocity.GetSafeNormal();
}

void UGA_EnemyBowShot::OnMontageCompleted()
{
	if (ShotStage == EEnemyBowShotStage::DrawIntro)
	{
		StartDrawHold();
		return;
	}

	if (ShotStage == EEnemyBowShotStage::Fire)
	{
		Super::OnMontageCompleted();
	}
}

void UGA_EnemyBowShot::OnMontageInterrupted()
{
	// DrawnStart의 정상 BlendOut에서 Hold 몽타주로 교체한 뒤 도착한 이전 task 콜백은 무시한다.
	if (ShotStage == EEnemyBowShotStage::DrawHold || ShotStage == EEnemyBowShotStage::DrawShake)
	{
		return;
	}

	Super::OnMontageInterrupted();
}

void UGA_EnemyBowShot::CacheBowMesh()
{
	CachedBowMeshComponent.Reset();
	CachedBowMeshAnimInstance.Reset();

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor))
	{
		return;
	}

	TArray<USkeletalMeshComponent*> SkeletalMeshes;
	AvatarActor->GetComponents<USkeletalMeshComponent>(SkeletalMeshes);
	for (USkeletalMeshComponent* SkeletalMesh : SkeletalMeshes)
	{
		if (!IsValid(SkeletalMesh))
		{
			continue;
		}

		if (URetrieveBowMeshAnimInstance* BowAnim = Cast<URetrieveBowMeshAnimInstance>(SkeletalMesh->GetAnimInstance()))
		{
			CachedBowMeshComponent = SkeletalMesh;
			CachedBowMeshAnimInstance = BowAnim;
			return;
		}
	}
}

void UGA_EnemyBowShot::FaceTarget() const
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	AActor* TargetActor = GetCachedTargetActor();
	if (!IsValid(AvatarActor) || !IsValid(TargetActor))
	{
		return;
	}

	FVector Direction = TargetActor->GetActorLocation() - AvatarActor->GetActorLocation();
	Direction.Z = 0.f;
	if (!Direction.IsNearlyZero())
	{
		AvatarActor->SetActorRotation(Direction.Rotation());
	}
}

void UGA_EnemyBowShot::StartDrawHold()
{
	if (!IsActive() || ShotStage != EEnemyBowShotStage::DrawIntro)
	{
		return;
	}

	ShotStage = EEnemyBowShotStage::DrawHold;
	PlayCharacterPhase(EBowShotPhase::Drawn, false);
	PlayBowMeshPhase(EBowShotPhase::Drawn);
	ScheduleNextPhase(DrawHoldDuration, &UGA_EnemyBowShot::StartDrawShake);
}

void UGA_EnemyBowShot::StartDrawShake()
{
	if (!IsActive() || ShotStage != EEnemyBowShotStage::DrawHold)
	{
		return;
	}

	if (DrawShakeDuration <= 0.f)
	{
		StartFire();
		return;
	}

	ShotStage = EEnemyBowShotStage::DrawShake;
	PlayCharacterPhase(EBowShotPhase::DrawnShake, false);
	PlayBowMeshPhase(EBowShotPhase::DrawnShake);
	ScheduleNextPhase(DrawShakeDuration, &UGA_EnemyBowShot::StartFire);
}

void UGA_EnemyBowShot::StartFire()
{
	if (!IsActive()
		|| (ShotStage != EEnemyBowShotStage::DrawHold && ShotStage != EEnemyBowShotStage::DrawShake))
	{
		return;
	}

	ShotStage = EEnemyBowShotStage::Fire;
	FaceTarget();

	const EBowShotPhase FirePhase = bUseFireReloadMontage
		? EBowShotPhase::FireReload
		: EBowShotPhase::FireIdle;
	const bool bHasFireMontage = CharacterShotMontages.Resolve(FirePhase, false) != nullptr;

	ScheduleProjectiles(bHasFireMontage);
	PlayBowMeshPhase(FirePhase);

	if (!PlayCharacterPhase(FirePhase, true) && bHasFireMontage)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void UGA_EnemyBowShot::ScheduleNextPhase(
	float BaseDuration,
	void (UGA_EnemyBowShot::*Callback)())
{
	UWorld* World = GetWorld();
	if (!World)
	{
		(this->*Callback)();
		return;
	}

	const float AttackSpeed = FMath::Max(0.01f, GetAttackSpeedMultiplier());
	const float Delay = FMath::Max(0.f, BaseDuration) / AttackSpeed;
	if (Delay <= KINDA_SMALL_NUMBER)
	{
		(this->*Callback)();
		return;
	}

	World->GetTimerManager().SetTimer(
		PhaseTimerHandle,
		FTimerDelegate::CreateUObject(this, Callback),
		Delay,
		false);
}

bool UGA_EnemyBowShot::PlayCharacterPhase(EBowShotPhase Phase, bool bWaitForCompletion)
{
	UAnimMontage* Montage = CharacterShotMontages.Resolve(Phase, false);
	if (!IsValid(Montage))
	{
		return false;
	}

	StopPhaseMontageTask();
	PhaseMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		Montage,
		GetAttackMontagePlayRate(1.f),
		NAME_None,
		true);
	if (!IsValid(PhaseMontageTask))
	{
		return false;
	}

	if (bWaitForCompletion)
	{
		PhaseMontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleFireMontageFinished);
		PhaseMontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleFireMontageFinished);
		PhaseMontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleFireMontageInterrupted);
		PhaseMontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleFireMontageInterrupted);
	}

	PhaseMontageTask->ReadyForActivation();
	return true;
}

void UGA_EnemyBowShot::PlayBowMeshPhase(EBowShotPhase Phase) const
{
	URetrieveBowMeshAnimInstance* BowAnim = CachedBowMeshAnimInstance.Get();
	if (!IsValid(BowAnim))
	{
		return;
	}

	if (UAnimMontage* Montage = BowAnim->ShotMontages.Resolve(Phase, false))
	{
		BowAnim->Montage_Play(Montage, GetAttackMontagePlayRate(1.f));
	}
}

void UGA_EnemyBowShot::StopPhaseMontageTask()
{
	if (PhaseMontageTask)
	{
		PhaseMontageTask->EndTask();
		PhaseMontageTask = nullptr;
	}
}

void UGA_EnemyBowShot::HandleFireMontageFinished()
{
	if (ShotStage == EEnemyBowShotStage::Fire)
	{
		OnMontageCompleted();
	}
}

void UGA_EnemyBowShot::HandleFireMontageInterrupted()
{
	if (ShotStage == EEnemyBowShotStage::Fire)
	{
		OnMontageInterrupted();
	}
}
