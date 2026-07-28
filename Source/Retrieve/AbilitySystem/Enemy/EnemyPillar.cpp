#include "AbilitySystem/Enemy/EnemyPillar.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Combat/RetrieveKnockbackLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GenericTeamAgentInterface.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

AEnemyPillar::AEnemyPillar()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	WarningDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("WarningDecal"));
	WarningDecal->SetupAttachment(SceneRoot);
	WarningDecal->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));

	PillarRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PillarRoot"));
	PillarRoot->SetupAttachment(SceneRoot);

	EffectArea = CreateDefaultSubobject<UCapsuleComponent>(TEXT("EffectArea"));
	EffectArea->SetupAttachment(PillarRoot);
	EffectArea->InitCapsuleSize(80.f, 180.f);
	EffectArea->SetCollisionObjectType(ECC_WorldDynamic);
	EffectArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EffectArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	EffectArea->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	EffectArea->SetGenerateOverlapEvents(true);

	PillarMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PillarMesh"));
	PillarMesh->SetupAttachment(EffectArea);
	PillarMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	RetentionArea = CreateDefaultSubobject<UCapsuleComponent>(TEXT("RetentionArea"));
	RetentionArea->SetupAttachment(EffectArea);
	RetentionArea->InitCapsuleSize(100.f, 200.f);
	RetentionArea->SetCollisionObjectType(ECC_WorldDynamic);
	RetentionArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RetentionArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	RetentionArea->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	RetentionArea->SetGenerateOverlapEvents(true);

	RiseVFXComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("RiseVFXComponent"));
	RiseVFXComponent->SetupAttachment(EffectArea);
	RiseVFXComponent->SetAutoActivate(false);
}

void AEnemyPillar::SetLaunchKnockbackConfig(
	const FMonsterLaunchKnockbackConfig& InLaunchKnockbackConfig)
{
	LaunchKnockbackConfig = InLaunchKnockbackConfig;
}

void AEnemyPillar::BeginPlay()
{
	Super::BeginPlay();

	WarningDuration = FMath::Max(0.f, WarningDuration);
	RiseDuration = FMath::Max(0.f, RiseDuration);
	ActiveDuration = FMath::Max(0.f, ActiveDuration);
	ContactDamageMultiplier = FMath::Max(0.f, ContactDamageMultiplier);
	PeriodicDamageMultiplier = FMath::Max(0.f, PeriodicDamageMultiplier);
	PeriodicDamageInterval = FMath::Max(0.01f, PeriodicDamageInterval);

	const float CapsuleHalfHeight = EffectArea->GetScaledCapsuleHalfHeight();
	EffectAreaFinalRelativeLocation = EffectArea->GetRelativeLocation()
		+ FVector::UpVector * CapsuleHalfHeight;

	InitialWarningDecalSize = WarningDecal->DecalSize;
	EffectArea->OnComponentBeginOverlap.AddDynamic(this, &AEnemyPillar::OnEffectAreaBeginOverlap);
	RetentionArea->OnComponentEndOverlap.AddDynamic(this, &AEnemyPillar::OnRetentionAreaEndOverlap);

	EnterPhase(EEnemyPillarPhase::Warning);
}

void AEnemyPillar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdatePhase(DeltaSeconds);
}

void AEnemyPillar::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PeriodicDamageTimerHandle);
	}

	TrackedContactActors.Empty();

	Super::EndPlay(EndPlayReason);
}

void AEnemyPillar::EnterPhase(EEnemyPillarPhase NewPhase)
{
	const bool bWasEffectPhase = IsEffectPhase(CurrentPhase);
	const bool bWillBeEffectPhase = IsEffectPhase(NewPhase);

	CurrentPhase = NewPhase;
	PhaseElapsedTime = 0.f;

	if (WarningDecal)
	{
		const bool bShouldShowWarningDecal =
			CurrentPhase == EEnemyPillarPhase::Warning
			|| CurrentPhase == EEnemyPillarPhase::Rising;
		WarningDecal->SetVisibility(bShouldShowWarningDecal);
	}

	if (PillarRoot)
	{
		PillarRoot->SetVisibility(bWillBeEffectPhase, true);
	}

	if (RetentionArea)
	{
		RetentionArea->SetCollisionEnabled(
			bWillBeEffectPhase ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}

	if (EffectArea)
	{
		EffectArea->SetCollisionEnabled(
			bWillBeEffectPhase ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}

	if (CurrentPhase == EEnemyPillarPhase::Warning)
	{
		WarningDecal->DecalSize = InitialWarningDecalSize;
		WarningDecal->MarkRenderStateDirty();
		EffectArea->SetRelativeLocation(
			EffectAreaFinalRelativeLocation
			- FVector::UpVector * EffectArea->GetScaledCapsuleHalfHeight() * 2.f);
	}
	else if (CurrentPhase == EEnemyPillarPhase::Rising)
	{
		EffectArea->SetRelativeLocation(
			EffectAreaFinalRelativeLocation
			- FVector::UpVector * EffectArea->GetScaledCapsuleHalfHeight() * 2.f);
		if (WarningDecal)
		{
			WarningDecal->SetFadeOut(0.f, RiseDuration, false);
		}
		if (RiseVFXComponent)
		{
			RiseVFXComponent->Activate(true);
		}
	}
	else if (CurrentPhase == EEnemyPillarPhase::Active)
	{
		EffectArea->SetRelativeLocation(EffectAreaFinalRelativeLocation);
		if (RiseVFXComponent)
		{
			RiseVFXComponent->Deactivate();
		}
	}

	UWorld* World = GetWorld();
	if (World)
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		if (bWasEffectPhase && !bWillBeEffectPhase)
		{
			TimerManager.ClearTimer(PeriodicDamageTimerHandle);
		}
		else if (!bWasEffectPhase && bWillBeEffectPhase
			&& HasAuthority() && PeriodicDamageEffectClass)
		{
			TimerManager.SetTimer(
				PeriodicDamageTimerHandle,
				this,
				&AEnemyPillar::ApplyPeriodicDamage,
				PeriodicDamageInterval,
				true);
		}
	}

	if (CurrentPhase == EEnemyPillarPhase::Finished)
	{
		if (PillarMesh)
		{
			PillarMesh->SetVisibility(false, true);
		}
		if (RiseVFXComponent)
		{
			RiseVFXComponent->Deactivate();
		}
		if (EndVFXSystem && PillarRoot)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				this,
				EndVFXSystem,
				PillarRoot->GetComponentLocation(),
				PillarRoot->GetComponentRotation());
		}
		if (IceBreakSFX && PillarRoot)
		{
			UGameplayStatics::PlaySoundAtLocation(this, IceBreakSFX, PillarRoot->GetComponentLocation());
		}
		ApplyPostExposureEffectToTrackedActors();
		Destroy();
	}
}

void AEnemyPillar::UpdatePhase(float DeltaSeconds)
{
	if (CurrentPhase == EEnemyPillarPhase::Finished)
	{
		return;
	}

	PhaseElapsedTime += FMath::Max(0.f, DeltaSeconds);
	const float Duration = GetPhaseDuration(CurrentPhase);
	const float PhaseAlpha = Duration > 0.f
		? FMath::Clamp(PhaseElapsedTime / Duration, 0.f, 1.f)
		: 1.f;

	if (CurrentPhase == EEnemyPillarPhase::Warning)
	{
		UpdateWarningVisual(PhaseAlpha);
	}
	else if (CurrentPhase == EEnemyPillarPhase::Rising)
	{
		UpdateRisingVisual(PhaseAlpha);
	}

	if (PhaseAlpha < 1.f)
	{
		return;
	}

	switch (CurrentPhase)
	{
	case EEnemyPillarPhase::Warning:
		EnterPhase(EEnemyPillarPhase::Rising);
		break;
	case EEnemyPillarPhase::Rising:
		EnterPhase(EEnemyPillarPhase::Active);
		break;
	case EEnemyPillarPhase::Active:
		EnterPhase(EEnemyPillarPhase::Finished);
		break;
	default:
		break;
	}
}

void AEnemyPillar::UpdateWarningVisual(float PhaseAlpha)
{
	if (!WarningDecal || !EffectArea)
	{
		return;
	}

	const float SmoothAlpha = FMath::SmoothStep(0.f, 1.f, PhaseAlpha);
	const float TargetRadius = EffectArea->GetScaledCapsuleRadius();
	FVector NewDecalSize = InitialWarningDecalSize;
	NewDecalSize.Y = FMath::Lerp(InitialWarningDecalSize.Y, TargetRadius, SmoothAlpha);
	NewDecalSize.Z = FMath::Lerp(InitialWarningDecalSize.Z, TargetRadius, SmoothAlpha);
	WarningDecal->DecalSize = NewDecalSize;
	WarningDecal->MarkRenderStateDirty();
}

void AEnemyPillar::UpdateRisingVisual(float PhaseAlpha)
{
	if (!EffectArea)
	{
		return;
	}

	const FVector StartLocation = EffectAreaFinalRelativeLocation
		- FVector::UpVector * EffectArea->GetScaledCapsuleHalfHeight() * 2.f;
	EffectArea->SetRelativeLocation(
		FMath::Lerp(StartLocation, EffectAreaFinalRelativeLocation, PhaseAlpha));
}

void AEnemyPillar::ApplyContactEffects(AActor* TargetActor)
{
	if (!HasAuthority() || !ShouldApplyEffectTo(TargetActor))
	{
		return;
	}

	if (ContactDamageEffectClass)
	{
		ApplyEffect(TargetActor, ContactDamageEffectClass, ContactDamageMultiplier);
	}

	ApplyLaunchKnockback(TargetActor);
}

void AEnemyPillar::ApplyPeriodicDamage()
{
	if (!HasAuthority() || !PeriodicDamageEffectClass || !IsEffectPhase(CurrentPhase))
	{
		return;
	}

	for (auto ActorIt = TrackedContactActors.CreateIterator(); ActorIt; ++ActorIt)
	{
		AActor* TargetActor = ActorIt->Get();
		if (!IsValid(TargetActor))
		{
			ActorIt.RemoveCurrent();
			continue;
		}

		if (ShouldApplyEffectTo(TargetActor))
		{
			ApplyEffect(TargetActor, PeriodicDamageEffectClass, PeriodicDamageMultiplier);
		}
	}
}

void AEnemyPillar::ApplyPostExposureEffect(AActor* TargetActor)
{
	if (!HasAuthority() || !PostExposureEffectClass || !ShouldApplyEffectTo(TargetActor))
	{
		return;
	}

	ApplyEffect(TargetActor, PostExposureEffectClass);
}

void AEnemyPillar::ApplyPostExposureEffectToTrackedActors()
{
	if (!HasAuthority())
	{
		return;
	}

	for (const TWeakObjectPtr<AActor>& ActorPtr : TrackedContactActors)
	{
		if (AActor* TargetActor = ActorPtr.Get(); IsValid(TargetActor))
		{
			ApplyPostExposureEffect(TargetActor);
		}
	}

	TrackedContactActors.Empty();
}

void AEnemyPillar::ApplyLaunchKnockback(AActor* TargetActor) const
{
	if (!HasAuthority() || !LaunchKnockbackConfig.bUseLaunchKnockback)
	{
		return;
	}

	ACharacter* TargetCharacter = Cast<ACharacter>(TargetActor);
	if (!TargetCharacter)
	{
		return;
	}

	FRetrieveKnockbackParams KnockbackParams;
	KnockbackParams.Strength = LaunchKnockbackConfig.KnockbackStrength;
	KnockbackParams.UpwardStrength = LaunchKnockbackConfig.KnockbackUpwardStrength;

	FVector KnockbackDirection = TargetCharacter->GetActorLocation()
		- EffectArea->GetComponentLocation();
	KnockbackDirection.Z = 0.f;

	if (!KnockbackDirection.Normalize())
	{
		const AActor* SourceActor = GetOwner() ? GetOwner() : GetInstigator();
		if (SourceActor)
		{
			KnockbackDirection = TargetCharacter->GetActorLocation()
				- SourceActor->GetActorLocation();
			KnockbackDirection.Z = 0.f;
			KnockbackDirection.Normalize();
		}
	}

	if (KnockbackDirection.IsNearlyZero())
	{
		KnockbackDirection = TargetCharacter->GetActorForwardVector().GetSafeNormal2D();
	}

	URetrieveKnockbackLibrary::ApplyKnockbackInDirection(
		TargetCharacter,
		KnockbackDirection,
		KnockbackParams);
}

bool AEnemyPillar::ApplyEffect(
	AActor* TargetActor,
	TSubclassOf<UGameplayEffect> EffectClass,
	float DamageMultiplier)
{
	if (!HasAuthority() || !EffectClass || !ShouldApplyEffectTo(TargetActor))
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

	const FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(EffectClass, 1.f, Context);
	if (!Spec.IsValid())
	{
		return false;
	}

	if (DamageMultiplier >= 0.f)
	{
		Spec.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Damage_Mul, DamageMultiplier);
	}

	SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	return true;
}

bool AEnemyPillar::ShouldApplyEffectTo(const AActor* OtherActor) const
{
	const AActor* SourceActor = GetOwner() ? GetOwner() : GetInstigator();
	if (!SourceActor || !OtherActor || OtherActor == this || OtherActor == SourceActor)
	{
		return false;
	}

	return FGenericTeamId::GetAttitude(SourceActor, OtherActor) == ETeamAttitude::Hostile;
}

bool AEnemyPillar::IsEffectPhase(EEnemyPillarPhase Phase) const
{
	return Phase == EEnemyPillarPhase::Rising || Phase == EEnemyPillarPhase::Active;
}

float AEnemyPillar::GetPhaseDuration(EEnemyPillarPhase Phase) const
{
	switch (Phase)
	{
	case EEnemyPillarPhase::Warning:
		return WarningDuration;
	case EEnemyPillarPhase::Rising:
		return RiseDuration;
	case EEnemyPillarPhase::Active:
		return ActiveDuration;
	default:
		return 0.f;
	}
}

void AEnemyPillar::OnEffectAreaBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!HasAuthority() || !IsEffectPhase(CurrentPhase) || !ShouldApplyEffectTo(OtherActor))
	{
		return;
	}

	const TWeakObjectPtr<AActor> ActorKey(OtherActor);
	if (!TrackedContactActors.Contains(ActorKey))
	{
		TrackedContactActors.Add(ActorKey);
		ApplyContactEffects(OtherActor);
	}
}

void AEnemyPillar::OnRetentionAreaEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex)
{
	if (!HasAuthority() || !OtherActor)
	{
		return;
	}

	const TWeakObjectPtr<AActor> ActorKey(OtherActor);
	if (TrackedContactActors.Remove(ActorKey) > 0)
	{
		ApplyPostExposureEffect(OtherActor);
	}
}
