#include "AbilitySystem/Enemy/EnemyGroundHazard.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Components/AudioComponent.h"
#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "GenericTeamAgentInterface.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "GameFramework/RootMotionSource.h"

AEnemyGroundHazard::AEnemyGroundHazard()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	DamageArea = CreateDefaultSubobject<USphereComponent>(TEXT("DamageArea"));
	DamageArea->SetupAttachment(SceneRoot);
	DamageArea->SetCollisionObjectType(ECC_WorldDynamic);
	DamageArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DamageArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	DamageArea->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	DamageArea->SetGenerateOverlapEvents(true);

	CenterDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("CenterDecal"));
	CenterDecal->SetupAttachment(SceneRoot);
	CenterDecal->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));

	ExpandingDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("ExpandingDecal"));
	ExpandingDecal->SetupAttachment(SceneRoot);
	ExpandingDecal->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));

	ExpandingVFXComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ExpandingVFXComponent"));
	ExpandingVFXComponent->SetupAttachment(SceneRoot);
	ExpandingVFXComponent->SetAutoActivate(false);
	ExpandingVFXComponent->SetHiddenInGame(true);

	LoopSFXComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("LoopSFXComponent"));
	LoopSFXComponent->SetupAttachment(SceneRoot);
	LoopSFXComponent->SetAutoActivate(false);
}

void AEnemyGroundHazard::BeginPlay()
{
	Super::BeginPlay();

	InitialRadius = FMath::Max(0.f, InitialRadius);
	MaxRadius = FMath::Max(InitialRadius, MaxRadius);
	CenterRadius = FMath::Clamp(CenterRadius, 0.f, MaxRadius);
	CollisionRadiusScale = FMath::Clamp(CollisionRadiusScale, 0.f, 1.f);
	DirectDamageInterval = FMath::Max(0.01f, DirectDamageInterval);
	PeriodicStatusDamageMultiplier = FMath::Max(0.f, PeriodicStatusDamageMultiplier);
	PostExposureDuration = FMath::Max(0.f, PostExposureDuration);
	VFXBaseRadius = FMath::Max(1.f, VFXBaseRadius);

	CenterMaterial = CenterDecal ? CenterDecal->CreateDynamicMaterialInstance() : nullptr;
	ExpandingMaterial = ExpandingDecal ? ExpandingDecal->CreateDynamicMaterialInstance() : nullptr;
	if (ExpandingVFXComponent)
	{
		InitialExpandingVFXScale = ExpandingVFXComponent->GetRelativeScale3D();
		if (ExpandingVFXSystem)
		{
			ExpandingVFXComponent->SetAsset(ExpandingVFXSystem);
		}
		ExpandingVFXComponent->SetRelativeLocation(FVector(0.f, 0.f, VFXHeightOffset));
		ExpandingVFXComponent->DeactivateImmediate();
		ExpandingVFXComponent->SetHiddenInGame(true);
	}
	DamageArea->OnComponentBeginOverlap.AddDynamic(this, &AEnemyGroundHazard::OnDamageAreaBeginOverlap);
	DamageArea->OnComponentEndOverlap.AddDynamic(this, &AEnemyGroundHazard::OnDamageAreaEndOverlap);

	CurrentRadius = InitialRadius;
	EnterPhase(EEnemyGroundHazardPhase::Warning);
}

void AEnemyGroundHazard::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdatePhase(DeltaSeconds);
	UpdateContinuousPushTargets();
}

void AEnemyGroundHazard::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PeriodicEffectTimerHandle);
	}
	ActorsInsideDamageArea.Empty();
	RemoveAllContinuousPushSources();
	if (ExpandingVFXComponent)
	{
		ExpandingVFXComponent->DeactivateImmediate();
	}
	if (LoopSFXComponent)
	{
		LoopSFXComponent->Stop();
	}

	Super::EndPlay(EndPlayReason);
}

void AEnemyGroundHazard::EnterPhase(EEnemyGroundHazardPhase NewPhase)
{
	const bool bWasDamagePhase = IsDamagePhase(CurrentPhase);
	const bool bWillBeDamagePhase = IsDamagePhase(NewPhase);
	if (bWasDamagePhase && !bWillBeDamagePhase)
	{
		RemoveAllContinuousPushSources();
		ApplyPostExposureEffectToTrackedActors();
	}

	CurrentPhase = NewPhase;
	PhaseElapsedTime = 0.f;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FTimerManager& TimerManager = World->GetTimerManager();

	const bool bDamageEnabled = IsDamagePhase(CurrentPhase);
	DamageArea->SetCollisionEnabled(bDamageEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);

	if (ExpandingVFXComponent)
	{
		const bool bShouldVFXBeActive = CurrentPhase == EEnemyGroundHazardPhase::Expanding
			|| CurrentPhase == EEnemyGroundHazardPhase::Active;
		if (bShouldVFXBeActive && ExpandingVFXComponent->GetAsset())
		{
			ExpandingVFXComponent->SetHiddenInGame(false);
			ExpandingVFXComponent->SetVisibility(true, true);
			ExpandingVFXComponent->Activate(true);
		}
		else
		{
			ExpandingVFXComponent->Deactivate();
			ExpandingVFXComponent->SetHiddenInGame(true);
		}
	}

	if (LoopSFXComponent)
	{
		const bool bShouldSFXBeActive = CurrentPhase == EEnemyGroundHazardPhase::Expanding
			|| CurrentPhase == EEnemyGroundHazardPhase::Active;
		if (bShouldSFXBeActive && LoopSFX)
		{
			if (!LoopSFXComponent->IsPlaying())
			{
				LoopSFXComponent->SetSound(LoopSFX);
				LoopSFXComponent->Play();
			}
		}
		else
		{
			LoopSFXComponent->FadeOut(0.3f, 0.f);
		}
	}

	if (NewPhase == EEnemyGroundHazardPhase::Expanding && SpawnSFX)
	{
		UGameplayStatics::PlaySoundAtLocation(this, SpawnSFX, GetActorLocation());
	}
	else if (NewPhase == EEnemyGroundHazardPhase::Finished && FadeOutSFX)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FadeOutSFX, GetActorLocation());
	}

	if (!bDamageEnabled)
	{
		TimerManager.ClearTimer(PeriodicEffectTimerHandle);
	}
	else if (!bWasDamagePhase && HasAuthority()
		&& (DirectDamageEffectClass || PeriodicStatusEffectClass))
	{
		ApplyPeriodicEffects();
		TimerManager.SetTimer(
			PeriodicEffectTimerHandle,
			this,
			&AEnemyGroundHazard::ApplyPeriodicEffects,
			DirectDamageInterval,
			true);
	}

	if (CurrentPhase == EEnemyGroundHazardPhase::Finished)
	{
		Destroy();
	}
}

void AEnemyGroundHazard::UpdatePhase(float DeltaSeconds)
{
	if (CurrentPhase == EEnemyGroundHazardPhase::Finished)
	{
		return;
	}

	PhaseElapsedTime += FMath::Max(0.f, DeltaSeconds);
	const float Duration = GetPhaseDuration(CurrentPhase);
	const float PhaseAlpha = Duration > 0.f
		? FMath::Clamp(PhaseElapsedTime / Duration, 0.f, 1.f)
		: 1.f;

	UpdateVisuals(PhaseAlpha);

	if (PhaseAlpha < 1.f)
	{
		return;
	}

	switch (CurrentPhase)
	{
	case EEnemyGroundHazardPhase::Warning:
		EnterPhase(EEnemyGroundHazardPhase::Expanding);
		break;
	case EEnemyGroundHazardPhase::Expanding:
		EnterPhase(EEnemyGroundHazardPhase::Active);
		break;
	case EEnemyGroundHazardPhase::Active:
		EnterPhase(EEnemyGroundHazardPhase::Fading);
		break;
	case EEnemyGroundHazardPhase::Fading:
		EnterPhase(EEnemyGroundHazardPhase::Finished);
		break;
	default:
		break;
	}
}

void AEnemyGroundHazard::UpdateVisuals(float PhaseAlpha)
{
	float Opacity = 1.f;
	if (CurrentPhase == EEnemyGroundHazardPhase::Warning)
	{
		Opacity = PhaseAlpha;
	}
	else if (CurrentPhase == EEnemyGroundHazardPhase::Fading)
	{
		Opacity = 1.f - PhaseAlpha;
	}

	if (CurrentPhase == EEnemyGroundHazardPhase::Expanding)
	{
		CurrentRadius = FMath::Lerp(InitialRadius, MaxRadius, PhaseAlpha);
	}
	else if (CurrentPhase == EEnemyGroundHazardPhase::Active
		|| CurrentPhase == EEnemyGroundHazardPhase::Fading)
	{
		CurrentRadius = MaxRadius;
	}
	else
	{
		CurrentRadius = InitialRadius;
	}

	SetDecalRadius(CenterDecal, CenterRadius);
	SetDecalRadius(ExpandingDecal, CurrentRadius);
	DamageArea->SetSphereRadius(GetCollisionRadius(), true);

	if (CenterMaterial)
	{
		CenterMaterial->SetScalarParameterValue(OpacityParameterName, Opacity);
	}
	if (ExpandingMaterial)
	{
		ExpandingMaterial->SetScalarParameterValue(OpacityParameterName, Opacity);
	}

	UpdateExpandingVFX();
}

void AEnemyGroundHazard::UpdateExpandingVFX()
{
	if (!ExpandingVFXComponent)
	{
		return;
	}

	ExpandingVFXComponent->SetRelativeLocation(FVector(0.f, 0.f, VFXHeightOffset));

	if (!bScaleVFXWithRadius)
	{
		return;
	}

	const float Scale = FMath::Max(0.01f, CurrentRadius / FMath::Max(1.f, VFXBaseRadius));
	ExpandingVFXComponent->SetRelativeScale3D(InitialExpandingVFXScale * Scale);
}

void AEnemyGroundHazard::SetDecalRadius(UDecalComponent* Decal, float Radius) const
{
	if (!Decal)
	{
		return;
	}

	const FVector NewSize(32.f, Radius, Radius);
	if (Decal->DecalSize.Equals(NewSize))
	{
		return;
	}

	Decal->DecalSize = NewSize;
	Decal->MarkRenderStateDirty();
}

void AEnemyGroundHazard::ApplyPeriodicEffects()
{
	if (!HasAuthority() || (!DirectDamageEffectClass && !PeriodicStatusEffectClass) || !DamageArea)
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	DamageArea->GetOverlappingActors(OverlappingActors);

	for (AActor* OtherActor : OverlappingActors)
	{
		if (!ShouldApplyDamageTo(OtherActor))
		{
			continue;
		}

		if (!IsInsideEffectiveArea(OtherActor))
		{
			continue;
		}

		ActorsInsideDamageArea.Add(OtherActor);
		ApplyContinuousPushTo(OtherActor);
		if (DirectDamageEffectClass)
		{
			ApplyEffect(
				OtherActor,
				DirectDamageEffectClass,
				DirectDamageMultiplier);
		}
		if (PeriodicStatusEffectClass)
		{
			ApplyEffect(
				OtherActor,
				PeriodicStatusEffectClass,
				PeriodicStatusDamageMultiplier);
		}
	}
}

void AEnemyGroundHazard::ApplyPostExposureEffect(AActor* TargetActor)
{
	if (!HasAuthority() || !PostExposureEffectClass || PostExposureDuration <= 0.f)
	{
		return;
	}

	ApplyEffect(
		TargetActor,
		PostExposureEffectClass,
		PostExposureDamageMultiplier,
		PostExposureDuration);
}

void AEnemyGroundHazard::ApplyPostExposureEffectToTrackedActors()
{
	if (!HasAuthority())
	{
		ActorsInsideDamageArea.Empty();
		return;
	}

	for (const TWeakObjectPtr<AActor>& ActorPtr : ActorsInsideDamageArea)
	{
		if (AActor* TargetActor = ActorPtr.Get())
		{
			ApplyPostExposureEffect(TargetActor);
		}
	}

	ActorsInsideDamageArea.Empty();
}

bool AEnemyGroundHazard::ApplyEffect(
	AActor* TargetActor,
	TSubclassOf<UGameplayEffect> EffectClass,
	float DamageMultiplier,
	float DurationOverride)
{
	if (!HasAuthority() || !EffectClass || !ShouldApplyDamageTo(TargetActor))
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
	if (DurationOverride >= 0.f)
	{
		Spec.Data->SetDuration(DurationOverride, true);
	}

	SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	return true;
}

void AEnemyGroundHazard::OnDamageAreaBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!HasAuthority() || !IsDamagePhase(CurrentPhase) || !ShouldApplyDamageTo(OtherActor))
	{
		return;
	}

	if (IsInsideEffectiveArea(OtherActor))
	{
		ActorsInsideDamageArea.Add(OtherActor);
		ApplyContinuousPushTo(OtherActor);
		if (PeriodicStatusEffectClass)
		{
			ApplyEffect(
				OtherActor,
				PeriodicStatusEffectClass,
				PeriodicStatusDamageMultiplier);
		}
	}
}

void AEnemyGroundHazard::OnDamageAreaEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex)
{
	if (!HasAuthority() || !OtherActor)
	{
		return;
	}

	RemoveContinuousPushFrom(OtherActor);

	const TWeakObjectPtr<AActor> ActorKey(OtherActor);
	if (ActorsInsideDamageArea.Remove(ActorKey) > 0 && IsDamagePhase(CurrentPhase))
	{
		ApplyPostExposureEffect(OtherActor);
	}
}

bool AEnemyGroundHazard::ShouldApplyDamageTo(const AActor* OtherActor) const
{
	const AActor* SourceActor = GetOwner() ? GetOwner() : GetInstigator();
	if (!SourceActor || !OtherActor || OtherActor == this || OtherActor == SourceActor)
	{
		return false;
	}

	return FGenericTeamId::GetAttitude(SourceActor, OtherActor) == ETeamAttitude::Hostile;
}

bool AEnemyGroundHazard::IsInsideEffectiveArea(const AActor* TargetActor) const
{
	if (!TargetActor)
	{
		return false;
	}

	FVector TargetLocation = TargetActor->GetActorLocation();
	if (const ACharacter* TargetCharacter = Cast<ACharacter>(TargetActor))
	{
		if (const UCharacterMovementComponent* MovementComponent =
			TargetCharacter->GetCharacterMovement())
		{
			TargetLocation = MovementComponent->GetActorFeetLocation();
		}
	}

	const FVector Offset = TargetLocation - GetActorLocation();
	return FMath::Abs(Offset.Z) <= MaxVerticalDifference
		&& FVector2D(Offset.X, Offset.Y).SizeSquared()
			<= FMath::Square(GetCollisionRadius());
}

bool AEnemyGroundHazard::IsDamagePhase(EEnemyGroundHazardPhase Phase) const
{
	return Phase == EEnemyGroundHazardPhase::Expanding
		|| Phase == EEnemyGroundHazardPhase::Active;
}

float AEnemyGroundHazard::GetPhaseDuration(EEnemyGroundHazardPhase Phase) const
{
	switch (Phase)
	{
	case EEnemyGroundHazardPhase::Warning:
		return WarningDuration;
	case EEnemyGroundHazardPhase::Expanding:
		return ExpandDuration;
	case EEnemyGroundHazardPhase::Active:
		return ActiveDuration;
	case EEnemyGroundHazardPhase::Fading:
		return FadeDuration;
	default:
		return 0.f;
	}
}

float AEnemyGroundHazard::GetCollisionRadius() const
{
	return FMath::Max(0.f, CurrentRadius * CollisionRadiusScale);
}

void AEnemyGroundHazard::ApplyContinuousPushTo(AActor* TargetActor)
{
	if (!HasAuthority() || !bUseContinuousPush
		|| ContinuousPushSpeed <= 0.f)
	{
		return;
	}

	ACharacter* TargetCharacter = Cast<ACharacter>(TargetActor);

	UCharacterMovementComponent* Movement = TargetCharacter
		? TargetCharacter->GetCharacterMovement() : nullptr;

	if (!Movement
		|| ActivePushRootMotionSourceIds.Contains(TargetCharacter))
	{
		return;
	}

	TSharedPtr<FRootMotionSource_RadialForce> Source =
		MakeShared<FRootMotionSource_RadialForce>();

	Source->InstanceName = FName(*FString::Printf(
		TEXT("GroundHazardPush_%u"),
		GetUniqueID()));

	Source->AccumulateMode =
		ERootMotionAccumulateMode::Additive;

	Source->Priority = 5;
	Source->Duration = -1.f;

	Source->LocationActor = this;
	Source->Radius = MaxRadius * CollisionRadiusScale;
	Source->Strength = ContinuousPushSpeed;

	Source->bIsPush = true;
	Source->bNoZForce = true;

	const uint16 SourceId =
		Movement->ApplyRootMotionSource(Source);

	ActivePushRootMotionSourceIds.Add(
		TargetCharacter,
		SourceId);
}

void AEnemyGroundHazard::UpdateContinuousPushTargets()
{
	if (!HasAuthority())
	{
		return;
	}

	if (!bUseContinuousPush || ContinuousPushSpeed <= 0.f
		|| !IsDamagePhase(CurrentPhase) || !DamageArea)
	{
		RemoveAllContinuousPushSources();
		return;
	}

	TArray<AActor*> OverlappingActors;
	DamageArea->GetOverlappingActors(OverlappingActors);
	for (AActor* OtherActor : OverlappingActors)
	{
		if (ShouldApplyDamageTo(OtherActor) && IsInsideEffectiveArea(OtherActor))
		{
			ApplyContinuousPushTo(OtherActor);
		}
	}

	for (auto SourceIt = ActivePushRootMotionSourceIds.CreateIterator(); SourceIt; ++SourceIt)
	{
		ACharacter* TargetCharacter = SourceIt.Key().Get();
		const bool bShouldKeepSource = IsValid(TargetCharacter)
			&& DamageArea->IsOverlappingActor(TargetCharacter)
			&& ShouldApplyDamageTo(TargetCharacter)
			&& IsInsideEffectiveArea(TargetCharacter);
		if (bShouldKeepSource)
		{
			continue;
		}

		if (TargetCharacter)
		{
			if (UCharacterMovementComponent* MovementComponent =
				TargetCharacter->GetCharacterMovement())
			{
				MovementComponent->RemoveRootMotionSourceByID(SourceIt.Value());
			}
		}
		SourceIt.RemoveCurrent();
	}
}

void AEnemyGroundHazard::RemoveContinuousPushFrom(AActor* TargetActor)
{
	ACharacter* TargetCharacter = Cast<ACharacter>(TargetActor);
	if (!TargetCharacter)
	{
		return;
	}

	const uint16* SourceId =
		ActivePushRootMotionSourceIds.Find(TargetCharacter);

	if (!SourceId)
	{
		return;
	}

	if (UCharacterMovementComponent* Movement =
		TargetCharacter->GetCharacterMovement())
	{
		Movement->RemoveRootMotionSourceByID(*SourceId);
	}

	ActivePushRootMotionSourceIds.Remove(TargetCharacter);
}

void AEnemyGroundHazard::RemoveAllContinuousPushSources()
{
	for (const TPair<TWeakObjectPtr<ACharacter>, uint16>& Pair
		: ActivePushRootMotionSourceIds)
	{
		ACharacter* Character = Pair.Key.Get();
		if (!Character)
		{
			continue;
		}

		if (UCharacterMovementComponent* Movement =
			Character->GetCharacterMovement())
		{
			Movement->RemoveRootMotionSourceByID(Pair.Value);
		}
	}

	ActivePushRootMotionSourceIds.Empty();
}
