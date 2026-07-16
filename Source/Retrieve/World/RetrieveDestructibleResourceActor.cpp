#include "World/RetrieveDestructibleResourceActor.h"

#include "Collision/RetrieveCollisionChannels.h"
#include "Components/StaticMeshComponent.h"
#include "Components/World/RetrieveInteractionResponseComponent.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Logging/RetrieveLogChannels.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

ARetrieveDestructibleResourceActor::ARetrieveDestructibleResourceActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	IntactMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("IntactMesh"));
	IntactMesh->SetupAttachment(SceneRoot);
	IntactMesh->SetVisibility(true);
	IntactMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	IntactMesh->SetCollisionObjectType(RetrieveCollisionChannels::Gatherable);

	FracturedMesh = CreateDefaultSubobject<UGeometryCollectionComponent>(TEXT("FracturedMesh"));
	FracturedMesh->SetupAttachment(SceneRoot);
	FracturedMesh->SetVisibility(false);
	FracturedMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FracturedMesh->SetSimulatePhysics(false);

	RewardComponent = CreateDefaultSubobject<URetrieveInteractionResponseComponent>(TEXT("RewardComponent"));
	RewardComponent->bAutoBindInteractionManager = false;
	RewardComponent->bDestroyOwnerOnApplied = false;
}

void ARetrieveDestructibleResourceActor::BeginPlay()
{
	Super::BeginPlay();
	if (IntactMesh)
	{
		HitBaseRelativeTransform = IntactMesh->GetRelativeTransform();
		bHitBaseTransformCaptured = true;
	}
	CreateCrackMIDs();
}

void ARetrieveDestructibleResourceActor::CreateCrackMIDs()
{
	CrackMIDs.Reset();
	if (!IntactMesh)
	{
		return;
	}

	const int32 NumMaterials = IntactMesh->GetNumMaterials();
	for (int32 SlotIndex = 0; SlotIndex < NumMaterials; ++SlotIndex)
	{
		if (UMaterialInstanceDynamic* MID = IntactMesh->CreateAndSetMaterialInstanceDynamic(SlotIndex))
		{
			CrackMIDs.Add(MID);
		}
	}
	UpdateCrackProgress(0.0f);
}

void ARetrieveDestructibleResourceActor::UpdateCrackProgress(float Progress)
{
	if (CrackProgressParamName.IsNone())
	{
		return;
	}

	const float Clamped = FMath::Clamp(Progress, 0.0f, 1.0f);
	for (UMaterialInstanceDynamic* MID : CrackMIDs)
	{
		if (MID)
		{
			MID->SetScalarParameterValue(CrackProgressParamName, Clamped);
		}
	}
}

void ARetrieveDestructibleResourceActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopHitShake();
	Super::EndPlay(EndPlayReason);
}

void ARetrieveDestructibleResourceActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARetrieveDestructibleResourceActor, bBroken);
}

bool ARetrieveDestructibleResourceActor::ReceiveRetrieveAttackHit_Implementation(
	AActor* Attacker,
	const FHitResult& HitResult,
	FGameplayTag AttackTypeTag,
	FGameplayTag ElementTag)
{
	if (!HasAuthority() || bBroken || !IsValid(Attacker))
	{
		return false;
	}

	++CurrentHitCount;

	UE_LOG(LogRetrieveWorld, Verbose, TEXT("[Resource] Hit Resource=%s Attacker=%s Count=%d/%d"),
		*GetName(), *GetNameSafe(Attacker), CurrentHitCount, RequiredHitCount);

	if (CurrentHitCount >= RequiredHitCount)
	{
		BreakResource(Attacker, HitResult.ImpactPoint);
	}
	else
	{
		MulticastPlayHitFeedback(HitResult.ImpactPoint, HitResult.ImpactNormal, CurrentHitCount);
	}

	return true;
}

void ARetrieveDestructibleResourceActor::BreakResource(AActor* Attacker, const FVector& ImpactPoint)
{
	if (!HasAuthority() || bBroken)
	{
		UE_LOG(LogRetrieveWorld, Verbose, TEXT("[Resource] Ignored duplicate break Resource=%s"), *GetName());
		return;
	}

	bBroken = true;
	IntactMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (RewardComponent)
	{
		RewardComponent->HandleInteractionApplied(Attacker);
	}

	UE_LOG(LogRetrieveWorld, Log, TEXT("[Resource] Broken Resource=%s"), *GetName());

	MulticastPlayBreak(ImpactPoint);
	ForceNetUpdate();
	SetLifeSpan(BrokenLifeSpan);
}

void ARetrieveDestructibleResourceActor::ApplyBrokenVisual(const FVector& ImpactPoint)
{
	if (bBreakVisualPlayed)
	{
		return;
	}
	bBreakVisualPlayed = true;
	StopHitShake();

	IntactMesh->SetVisibility(false, true);
	IntactMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	FracturedMesh->SetVisibility(true, true);
	FracturedMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	FracturedMesh->SetSimulatePhysics(true);
	FracturedMesh->ApplyExternalStrain(INDEX_NONE, ImpactPoint, BreakRadius, 1, 1.0f, BreakStrain);

	// ApplyExternalStrain은 클러스터 결합을 끊을 뿐 힘을 주지 않으므로, 별도로 바깥 방향 임펄스를 줘서
	// 조각이 실제로 흩어지게 한다. bVelChange=true로 질량에 상관없이 일정한 튀는 정도를 유지한다.
	FracturedMesh->AddRadialImpulse(ImpactPoint, BreakRadius, BreakImpulseStrength, ERadialImpulseFalloff::RIF_Linear, /*bVelChange=*/true);

	PlayBreakFeedback(ImpactPoint);
}

void ARetrieveDestructibleResourceActor::OnRep_Broken()
{
	if (bBroken)
	{
		ApplyBrokenVisual(GetActorLocation());
	}
}

void ARetrieveDestructibleResourceActor::MulticastPlayHitFeedback_Implementation(FVector_NetQuantize ImpactPoint, FVector_NetQuantizeNormal ImpactNormal, int32 HitCount)
{
	const float HitProgress = RequiredHitCount > 0
		? static_cast<float>(HitCount) / static_cast<float>(RequiredHitCount)
		: 0.0f;

	UpdateCrackProgress(HitProgress);
	PlayDefaultHitFeedback(ImpactPoint, ImpactNormal, HitCount, HitProgress);
	PlayHitFeedback(ImpactPoint, ImpactNormal, HitCount, HitProgress);
}

void ARetrieveDestructibleResourceActor::PlayDefaultHitFeedback(
	const FVector& ImpactPoint,
	const FVector& ImpactNormal,
	int32 HitCount,
	float HitProgress)
{
	StartHitShake(ImpactNormal, HitCount, HitProgress);

	if (UNiagaraSystem* System = HitImpactVFX.LoadSynchronous())
	{
		const float ProgressScale = FMath::Lerp(0.85f, 1.15f, FMath::Clamp(HitProgress, 0.0f, 1.0f));
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			System,
			ImpactPoint,
			ImpactNormal.Rotation(),
			FVector(HitImpactVFXScale * ProgressScale));
	}

	if (USoundBase* Sound = HitImpactSound.LoadSynchronous())
	{
		const float Pitch = 0.96f + 0.025f * static_cast<float>(HitCount % 4);
		UGameplayStatics::PlaySoundAtLocation(this, Sound, ImpactPoint, 0.8f, Pitch);
	}
}

void ARetrieveDestructibleResourceActor::StartHitShake(
	const FVector& ImpactNormal,
	int32 HitCount,
	float HitProgress)
{
	UWorld* World = GetWorld();
	if (!IntactMesh || !World || HitShakeDuration <= 0.0f)
	{
		return;
	}

	if (!bHitBaseTransformCaptured)
	{
		HitBaseRelativeTransform = IntactMesh->GetRelativeTransform();
		bHitBaseTransformCaptured = true;
	}

	HitShakeLocalDirection = IntactMesh->GetComponentTransform()
		.InverseTransformVectorNoScale(-ImpactNormal)
		.GetSafeNormal();
	if (HitShakeLocalDirection.IsNearlyZero())
	{
		HitShakeLocalDirection = FVector::BackwardVector;
	}
	HitShakeStartTime = World->GetTimeSeconds();
	HitShakeStrength = FMath::Lerp(0.85f, 1.25f, FMath::Clamp(HitProgress, 0.0f, 1.0f));
	HitShakeSeed = HitCount;

	World->GetTimerManager().ClearTimer(HitShakeTimer);
	World->GetTimerManager().SetTimer(
		HitShakeTimer,
		this,
		&ARetrieveDestructibleResourceActor::UpdateHitShake,
		1.0f / 60.0f,
		true);
	UpdateHitShake();
}

void ARetrieveDestructibleResourceActor::UpdateHitShake()
{
	UWorld* World = GetWorld();
	if (!IntactMesh || !bHitBaseTransformCaptured || !World)
	{
		StopHitShake();
		return;
	}

	const float Elapsed = World->GetTimeSeconds() - HitShakeStartTime;
	const float Alpha = FMath::Clamp(Elapsed / HitShakeDuration, 0.0f, 1.0f);
	if (Alpha >= 1.0f)
	{
		StopHitShake();
		return;
	}

	const float Envelope = 1.0f - Alpha;
	const float Oscillation = FMath::Sin(Alpha * UE_PI * 5.0f) * Envelope * HitShakeStrength;
	const FVector Location = HitBaseRelativeTransform.GetLocation()
		+ HitShakeLocalDirection * HitShakeDistance * Oscillation;
	const float RotationSign = (HitShakeSeed & 1) == 0 ? 1.0f : -1.0f;
	const FRotator RotationOffset(
		HitShakeRotationDegrees * Oscillation,
		HitShakeRotationDegrees * 0.35f * Oscillation * RotationSign,
		HitShakeRotationDegrees * 0.7f * Oscillation * RotationSign);
	const FQuat Rotation = HitBaseRelativeTransform.GetRotation() * RotationOffset.Quaternion();

	// 스케일 스쿼시: 타격 직후 살짝 눌렸다가(Envelope=1) 원래 크기로 복원(Envelope=0)된다.
	const float ScaleFactor = 1.0f - HitScalePunch * Envelope * HitShakeStrength;
	const FVector NewScale = HitBaseRelativeTransform.GetScale3D() * FMath::Max(ScaleFactor, 0.1f);

	IntactMesh->SetRelativeTransform(FTransform(Rotation, Location, NewScale));
}

void ARetrieveDestructibleResourceActor::StopHitShake()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitShakeTimer);
	}
	if (IntactMesh && bHitBaseTransformCaptured)
	{
		IntactMesh->SetRelativeTransform(HitBaseRelativeTransform);
	}
}

void ARetrieveDestructibleResourceActor::MulticastPlayBreak_Implementation(FVector_NetQuantize ImpactPoint)
{
	ApplyBrokenVisual(ImpactPoint);
}
