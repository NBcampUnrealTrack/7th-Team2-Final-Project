#include "AbilitySystem/Enemy/EnemyWaveHazard.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GenericTeamAgentInterface.h"
#include "GameplayEffect.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "NiagaraComponent.h"
#include "Engine/OverlapResult.h"

AEnemyWaveHazard::AEnemyWaveHazard()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	EffectArea = CreateDefaultSubobject<UBoxComponent>(TEXT("EffectArea"));
	EffectArea->SetupAttachment(SceneRoot);
	EffectArea->SetBoxExtent(FVector(120.f, 300.f, 120.f));
	EffectArea->SetCollisionObjectType(ECC_WorldDynamic);
	EffectArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	EffectArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	EffectArea->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	EffectArea->SetGenerateOverlapEvents(true);

	WaveVFXComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("WaveVFXComponent"));
	WaveVFXComponent->SetupAttachment(EffectArea);
	WaveVFXComponent->SetAutoActivate(true);
}

void AEnemyWaveHazard::BeginPlay()
{
	Super::BeginPlay();

	MoveSpeed = FMath::Max(0.f, MoveSpeed);
	MoveAcceleration = FMath::Max(0.f, MoveAcceleration);
	MaxMoveSpeed = FMath::Max(0.f, MaxMoveSpeed);
	MaxTravelDistance = FMath::Max(0.f, MaxTravelDistance);
	CarrySpeed = FMath::Max(0.f, CarrySpeed);
	LifeTime = FMath::Max(0.f, LifeTime);
	DamageMultiplier = FMath::Max(0.f, DamageMultiplier);
	StatusEffectDamageMultiplier = FMath::Max(0.f, StatusEffectDamageMultiplier);
	DamageInterval = FMath::Max(0.01f, DamageInterval);

	CurrentMoveSpeed = MoveSpeed;
	InitialEffectAreaRelativeLocation = EffectArea->GetRelativeLocation();

	SetLifeSpan(LifeTime);
}

void AEnemyWaveHazard::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ElapsedTime += FMath::Max(0.f, DeltaSeconds);
	UpdateCollisionMovement(DeltaSeconds);
	DrawDebugCollision(DeltaSeconds);

	if (HasAuthority())
	{
		UpdateTrackedTargets();
		UpdateCarriedTargets();
	}

	if ((LifeTime > 0.f && ElapsedTime >= LifeTime)
		|| (MaxTravelDistance > 0.f && GetMovedDistance() >= MaxTravelDistance))
	{
		Destroy();
	}
}

void AEnemyWaveHazard::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ActiveTargets.Empty();

	Super::EndPlay(EndPlayReason);
}

void AEnemyWaveHazard::UpdateCollisionMovement(float DeltaSeconds)
{
	if (!EffectArea || (CurrentMoveSpeed <= 0.f && MoveAcceleration <= 0.f))
	{
		return;
	}

	const float ClampedDeltaSeconds = FMath::Max(0.f, DeltaSeconds);
	if (MoveAcceleration > 0.f)
	{
		CurrentMoveSpeed += MoveAcceleration * ClampedDeltaSeconds;
		if (MaxMoveSpeed > 0.f)
		{
			CurrentMoveSpeed = FMath::Min(CurrentMoveSpeed, MaxMoveSpeed);
		}
	}

	const float MoveDelta = CurrentMoveSpeed * ClampedDeltaSeconds;
	const FVector NewRelativeLocation = EffectArea->GetRelativeLocation()
		+ FVector::ForwardVector * MoveDelta;
	EffectArea->SetRelativeLocation(NewRelativeLocation);
}

void AEnemyWaveHazard::UpdateTrackedTargets()
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

void AEnemyWaveHazard::CollectCurrentTargets(TSet<TWeakObjectPtr<AActor>>& OutTargets) const
{
	OutTargets.Empty();

	if (!EffectArea || !GetWorld())
	{
		return;
	}

	TArray<FOverlapResult> OverlapResults;
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyWaveHazardOverlap), false, this);
	if (const AActor* SourceActor = GetOwner() ? GetOwner() : GetInstigator())
	{
		QueryParams.AddIgnoredActor(SourceActor);
	}

	const bool bHasOverlap = GetWorld()->OverlapMultiByObjectType(
		OverlapResults,
		EffectArea->GetComponentLocation(),
		EffectArea->GetComponentQuat(),
		ObjectQueryParams,
		FCollisionShape::MakeBox(EffectArea->GetScaledBoxExtent()),
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

void AEnemyWaveHazard::DrawDebugCollision(float DeltaSeconds)
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

	DrawDebugBox(
		GetWorld(),
		EffectArea->GetComponentLocation(),
		EffectArea->GetScaledBoxExtent(),
		EffectArea->GetComponentQuat(),
		FColor::Red,
		false,
		DebugDrawDuration,
		0,
		2.f);
}

void AEnemyWaveHazard::UpdateCarriedTargets()
{
	if (!bUseCarry || CarrySpeed <= 0.f)
	{
		return;
	}

	const FVector CarryDirection = GetActorForwardVector().GetSafeNormal2D();
	if (CarryDirection.IsNearlyZero())
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

		FVector NewVelocity = Movement->Velocity;
		NewVelocity.X = CarryDirection.X * CarrySpeed;
		NewVelocity.Y = CarryDirection.Y * CarrySpeed;
		Movement->Velocity = NewVelocity;
	}
}

bool AEnemyWaveHazard::ApplyDamageEffect(AActor* TargetActor)
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
		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
		Context.AddInstigator(SourceActor, this);

		const FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(StatusEffectClass, 1.f, Context);
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(
				RetrieveGameplayTags::Data_Damage_Mul,
				StatusEffectDamageMultiplier);
			SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
			bAppliedAnyEffect = true;
		}
	}

	return bAppliedAnyEffect;
}

bool AEnemyWaveHazard::ApplyStatusEffect(AActor* TargetActor)
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

bool AEnemyWaveHazard::ShouldAffectTarget(const AActor* TargetActor) const
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

float AEnemyWaveHazard::GetMovedDistance() const
{
	return EffectArea
		? FVector::Dist2D(InitialEffectAreaRelativeLocation, EffectArea->GetRelativeLocation())
		: 0.f;
}
