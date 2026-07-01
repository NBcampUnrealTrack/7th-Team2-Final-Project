#include "AbilitySystem/Enemy/EnemyTornadoHazard.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GenericTeamAgentInterface.h"
#include "GameplayEffect.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"

AEnemyTornadoHazard::AEnemyTornadoHazard()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	EffectArea = CreateDefaultSubobject<UCapsuleComponent>(TEXT("EffectArea"));
	EffectArea->SetupAttachment(SceneRoot);
	EffectArea->SetCapsuleRadius(250.f);
	EffectArea->SetCapsuleHalfHeight(500.f);
	EffectArea->SetCollisionObjectType(ECC_WorldDynamic);
	EffectArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	EffectArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	EffectArea->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	EffectArea->SetGenerateOverlapEvents(true);

	TornadoVFXComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("TornadoVFXComponent"));
	TornadoVFXComponent->SetupAttachment(EffectArea);
	TornadoVFXComponent->SetAutoActivate(true);
}

void AEnemyTornadoHazard::BeginPlay()
{
	Super::BeginPlay();

	TrackSpeed = FMath::Max(0.f, TrackSpeed);
	TargetSearchRadius = FMath::Max(0.f, TargetSearchRadius);
	TargetSearchInterval = FMath::Max(0.01f, TargetSearchInterval);
	PullSpeed = FMath::Max(0.f, PullSpeed);
	UpwardVelocity = FMath::Max(0.f, UpwardVelocity);
	LifeTime = FMath::Max(0.f, LifeTime);
	DamageMultiplier = FMath::Max(0.f, DamageMultiplier);
	StatusEffectDamageMultiplier = FMath::Max(0.f, StatusEffectDamageMultiplier);
	DamageInterval = FMath::Max(0.01f, DamageInterval);

	SetLifeSpan(LifeTime);
}

void AEnemyTornadoHazard::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float ClampedDeltaSeconds = FMath::Max(0.f, DeltaSeconds);
	ElapsedTime += ClampedDeltaSeconds;

	if (HasAuthority())
	{
		UpdateTrackingTarget(ClampedDeltaSeconds);
		MoveToTrackingTarget(ClampedDeltaSeconds);
		UpdateTrackedTargets();
		ApplyPullToTargets();
	}

	DrawDebugCollision(ClampedDeltaSeconds);

	if (LifeTime > 0.f && ElapsedTime >= LifeTime)
	{
		Destroy();
	}
}

void AEnemyTornadoHazard::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ActiveTargets.Empty();
	TrackingTarget.Reset();

	Super::EndPlay(EndPlayReason);
}

void AEnemyTornadoHazard::UpdateTrackingTarget(float DeltaSeconds)
{
	if (!bTrackTarget)
	{
		return;
	}

	TargetSearchElapsedTime += DeltaSeconds;
	if (TrackingTarget.IsValid() && TargetSearchElapsedTime < TargetSearchInterval)
	{
		return;
	}

	TargetSearchElapsedTime = 0.f;
	TrackingTarget = FindNearestTarget();
}

void AEnemyTornadoHazard::MoveToTrackingTarget(float DeltaSeconds)
{
	if (!bTrackTarget || TrackSpeed <= 0.f || !TrackingTarget.IsValid())
	{
		return;
	}

	const FVector CurrentLocation = GetActorLocation();
	const FVector TargetLocation = TrackingTarget->GetActorLocation();
	const FVector ToTarget = (TargetLocation - CurrentLocation).GetSafeNormal2D();
	if (ToTarget.IsNearlyZero())
	{
		return;
	}

	SetActorLocation(CurrentLocation + ToTarget * TrackSpeed * DeltaSeconds);
}

void AEnemyTornadoHazard::UpdateTrackedTargets()
{
	if (!DamageEffectClass && !StatusEffectClass)
	{
		ActiveTargets.Empty();
		return;
	}

	TSet<TWeakObjectPtr<AActor>> CurrentTargets;
	CollectCurrentTargets(CurrentTargets);

	for (const TWeakObjectPtr<AActor>& TargetPtr : CurrentTargets)
	{
		AActor* TargetActor = TargetPtr.Get();
		if (!IsValid(TargetActor) || !ShouldAffectTarget(TargetActor))
		{
			continue;
		}

		float* LastDamageTime = ActiveTargets.Find(TargetPtr);
		if (!LastDamageTime)
		{
			if (ApplyDamageEffect(TargetActor))
			{
				ActiveTargets.Add(TargetPtr, ElapsedTime);
			}
			continue;
		}

		if (ElapsedTime - *LastDamageTime >= DamageInterval)
		{
			if (ApplyDamageEffect(TargetActor))
			{
				*LastDamageTime = ElapsedTime;
			}
		}
	}

	for (auto TargetIt = ActiveTargets.CreateIterator(); TargetIt; ++TargetIt)
	{
		AActor* TargetActor = TargetIt.Key().Get();
		if (!IsValid(TargetActor))
		{
			TargetIt.RemoveCurrent();
			continue;
		}

		if (!CurrentTargets.Contains(TargetIt.Key()) || !ShouldAffectTarget(TargetActor))
		{
			if (bApplyStatusEffectOnExit)
			{
				ApplyStatusEffect(TargetActor);
			}
			TargetIt.RemoveCurrent();
		}
	}
}

void AEnemyTornadoHazard::CollectCurrentTargets(TSet<TWeakObjectPtr<AActor>>& OutTargets) const
{
	OutTargets.Empty();

	if (!EffectArea || !GetWorld())
	{
		return;
	}

	TArray<FOverlapResult> OverlapResults;
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyTornadoHazardOverlap), false, this);
	if (const AActor* SourceActor = GetOwner() ? GetOwner() : GetInstigator())
	{
		QueryParams.AddIgnoredActor(SourceActor);
	}

	const bool bHasOverlap = GetWorld()->OverlapMultiByObjectType(
		OverlapResults,
		EffectArea->GetComponentLocation(),
		EffectArea->GetComponentQuat(),
		ObjectQueryParams,
		FCollisionShape::MakeCapsule(
			EffectArea->GetScaledCapsuleRadius(),
			EffectArea->GetScaledCapsuleHalfHeight()),
		QueryParams);

	if (!bHasOverlap)
	{
		return;
	}

	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* TargetActor = OverlapResult.GetActor();
		if (IsValid(TargetActor) && ShouldAffectTarget(TargetActor))
		{
			OutTargets.Add(TargetActor);
		}
	}
}

void AEnemyTornadoHazard::ApplyPullToTargets()
{
	if (PullSpeed <= 0.f && UpwardVelocity <= 0.f)
	{
		return;
	}

	for (auto TargetIt = ActiveTargets.CreateIterator(); TargetIt; ++TargetIt)
	{
		AActor* TargetActor = TargetIt.Key().Get();
		if (!IsValid(TargetActor) || !ShouldAffectTarget(TargetActor))
		{
			TargetIt.RemoveCurrent();
			continue;
		}

		ACharacter* TargetCharacter = Cast<ACharacter>(TargetActor);
		UCharacterMovementComponent* Movement = TargetCharacter
			? TargetCharacter->GetCharacterMovement()
			: nullptr;
		if (!Movement)
		{
			continue;
		}

		const FVector ToCenter = (GetActorLocation() - TargetActor->GetActorLocation()).GetSafeNormal2D();
		FVector NewVelocity = Movement->Velocity;
		if (!ToCenter.IsNearlyZero())
		{
			NewVelocity.X = ToCenter.X * PullSpeed;
			NewVelocity.Y = ToCenter.Y * PullSpeed;
		}
		NewVelocity.Z = UpwardVelocity;
		Movement->Velocity = NewVelocity;
	}
}

bool AEnemyTornadoHazard::ApplyDamageEffect(AActor* TargetActor)
{
	if (!HasAuthority() || !ShouldAffectTarget(TargetActor))
	{
		return false;
	}

	AActor* SourceActor = GetOwner() ? GetOwner() : GetInstigator();
	IAbilitySystemInterface* SourceInterface = Cast<IAbilitySystemInterface>(SourceActor);
	IAbilitySystemInterface* TargetInterface = Cast<IAbilitySystemInterface>(TargetActor);
	UAbilitySystemComponent* SourceASC = SourceInterface ? SourceInterface->GetAbilitySystemComponent() : nullptr;
	UAbilitySystemComponent* TargetASC = TargetInterface ? TargetInterface->GetAbilitySystemComponent() : nullptr;
	if (!SourceASC || !TargetASC)
	{
		return false;
	}

	bool bAppliedAnyEffect = false;

	if (DamageEffectClass)
	{
		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
		Context.AddInstigator(SourceActor, this);

		const FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(DamageEffectClass, 1.f, Context);
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Damage_Mul, DamageMultiplier);
			SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
			bAppliedAnyEffect = true;
		}
	}

	if (bApplyStatusEffectOnDamageTick && StatusEffectClass)
	{
		bAppliedAnyEffect |= ApplyStatusEffect(TargetActor);
	}

	return bAppliedAnyEffect;
}

bool AEnemyTornadoHazard::ApplyStatusEffect(AActor* TargetActor)
{
	if (!HasAuthority() || !StatusEffectClass || !ShouldAffectTarget(TargetActor))
	{
		return false;
	}

	AActor* SourceActor = GetOwner() ? GetOwner() : GetInstigator();
	IAbilitySystemInterface* SourceInterface = Cast<IAbilitySystemInterface>(SourceActor);
	IAbilitySystemInterface* TargetInterface = Cast<IAbilitySystemInterface>(TargetActor);
	UAbilitySystemComponent* SourceASC = SourceInterface ? SourceInterface->GetAbilitySystemComponent() : nullptr;
	UAbilitySystemComponent* TargetASC = TargetInterface ? TargetInterface->GetAbilitySystemComponent() : nullptr;
	if (!SourceASC || !TargetASC)
	{
		return false;
	}

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddInstigator(SourceActor, this);

	const FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(StatusEffectClass, 1.f, Context);
	if (!Spec.IsValid())
	{
		return false;
	}

	Spec.Data->SetSetByCallerMagnitude(
		RetrieveGameplayTags::Data_Damage_Mul,
		StatusEffectDamageMultiplier);
	SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	return true;
}

bool AEnemyTornadoHazard::ShouldAffectTarget(const AActor* TargetActor) const
{
	const AActor* SourceActor = GetOwner() ? GetOwner() : GetInstigator();
	if (!TargetActor || TargetActor == this || TargetActor == SourceActor)
	{
		return false;
	}

	if (!bAffectHostileOnly)
	{
		return true;
	}

	if (!SourceActor)
	{
		return false;
	}

	return FGenericTeamId::GetAttitude(SourceActor, TargetActor) == ETeamAttitude::Hostile;
}

AActor* AEnemyTornadoHazard::FindNearestTarget() const
{
	if (!GetWorld())
	{
		return nullptr;
	}

	TArray<AActor*> Pawns;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APawn::StaticClass(), Pawns);

	AActor* NearestTarget = nullptr;
	float NearestDistanceSquared = FMath::Square(TargetSearchRadius);
	const FVector Origin = GetActorLocation();

	for (AActor* PawnActor : Pawns)
	{
		if (!IsValid(PawnActor) || !ShouldAffectTarget(PawnActor))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared2D(Origin, PawnActor->GetActorLocation());
		if (DistanceSquared <= NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			NearestTarget = PawnActor;
		}
	}

	return NearestTarget;
}

void AEnemyTornadoHazard::DrawDebugCollision(float DeltaSeconds)
{
	if (!bDrawDebugCollision || !EffectArea || !GetWorld())
	{
		return;
	}

	DebugDrawElapsedTime += FMath::Max(0.f, DeltaSeconds);
	if (DebugDrawElapsedTime < DebugDrawInterval)
	{
		return;
	}

	DebugDrawElapsedTime = 0.f;

	DrawDebugCapsule(
		GetWorld(),
		EffectArea->GetComponentLocation(),
		EffectArea->GetScaledCapsuleHalfHeight(),
		EffectArea->GetScaledCapsuleRadius(),
		EffectArea->GetComponentQuat(),
		FColor::Cyan,
		false,
		DebugDrawDuration,
		0,
		2.f);
}
