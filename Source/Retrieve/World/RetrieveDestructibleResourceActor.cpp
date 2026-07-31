#include "World/RetrieveDestructibleResourceActor.h"

#include "Collision/RetrieveCollisionChannels.h"
#include "Components/StaticMeshComponent.h"
#include "Components/World/RetrieveInteractionResponseComponent.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Logging/RetrieveLogChannels.h"
#include "Engine/GameInstance.h"
#include "Save/RetrieveSaveSubsystem.h"
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
		DefaultIntactCollision = IntactMesh->GetCollisionEnabled();
	}
	CreateCrackMIDs();

	if (!HasAuthority()) { return; }

	HandleSaveLoaded();   // 재시작 로드 시 이미 채굴된 자원이면 제거

	URetrieveSaveSubsystem* SaveSub = GetSaveSubsystem();
	if (!IsValid(SaveSub)) { return; }
	SaveSub->OnWorldObjectStatesChanged.AddUniqueDynamic(this, &ARetrieveDestructibleResourceActor::HandleSaveLoaded);
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

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BrokenHideTimerHandle);
	}

	// EndPlay는 Super를 항상 호출해야 하므로 early return 대신 단일 if만 사용.
	URetrieveSaveSubsystem* SaveSub = GetSaveSubsystem();
	if (IsValid(SaveSub))
	{
		SaveSub->OnWorldObjectStatesChanged.RemoveDynamic(this, &ARetrieveDestructibleResourceActor::HandleSaveLoaded);
	}

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

	// 세이브에 파괴 상태 기록(재시작 로드 시 이 자원은 제거됨).
	if (URetrieveSaveSubsystem* SaveSub = GetSaveSubsystem())
	{
		SaveSub->SetWorldObjectState(GetSaveId(), 1);
	}

	if (RewardComponent)
	{
		RewardComponent->HandleInteractionApplied(Attacker);
	}

	UE_LOG(LogRetrieveWorld, Log, TEXT("[Resource] Broken Resource=%s"), *GetName());

	MulticastPlayBreak(ImpactPoint);
	ForceNetUpdate();

	// Option A: Destroy(SetLifeSpan) 대신 일정 시간 뒤 숨김 — 액터를 복원용으로 유지.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(BrokenHideTimerHandle, this,
			&ARetrieveDestructibleResourceActor::HideBrokenResource, BrokenLifeSpan, false);
	}
}

FName ARetrieveDestructibleResourceActor::GetSaveId() const
{
	return PersistentId.IsNone() ? GetFName() : PersistentId;
}

URetrieveSaveSubsystem* ARetrieveDestructibleResourceActor::GetSaveSubsystem() const
{
	UGameInstance* GI = GetGameInstance();
	if (!IsValid(GI)) { return nullptr; }
	return GI->GetSubsystem<URetrieveSaveSubsystem>();
}

void ARetrieveDestructibleResourceActor::HandleSaveLoaded()
{
	if (!HasAuthority()) { return; }

	URetrieveSaveSubsystem* SaveSub = GetSaveSubsystem();
	if (!IsValid(SaveSub)) { return; }

	uint8 State = 0;
	const bool bWantBroken = SaveSub->TryGetWorldObjectState(GetSaveId(), State) && (State != 0);

	// 무조건 목표 상태 적용(파편 연출 중 로드 등 경계 케이스 안전).
	if (bWantBroken)
	{
		ApplyDepletedInstant();   // 채굴됨 → 즉시 고갈
	}
	else
	{
		ApplyIntactState();       // 미채굴 → 온전 복원
	}

	ForceNetUpdate();
}

void ARetrieveDestructibleResourceActor::HideBrokenResource()
{
	// 파괴 연출 종료 후 "채굴됨" 상태로 숨긴다(Destroy 대신). bHidden은 복제되어 클라도 숨겨진다.
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	if (FracturedMesh)
	{
		FracturedMesh->SetSimulatePhysics(false);
		FracturedMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ARetrieveDestructibleResourceActor::ApplyDepletedInstant()
{
	// 로드: 이미 채굴된 자원을 연출/보상 없이 즉시 고갈 상태로.
	// [MP TODO] bActorEnableCollision/이 함수 자체는 복제되지 않는다. 코업 도입 시 클라의 OnRep_Broken이
	// ApplyBrokenVisual로 FracturedMesh 충돌을 켜 "안 보이는 충돌체"가 남을 수 있음 →
	// Intact/Breaking/Depleted RepNotify 상태 복제 or Reliable NetMulticast로 양쪽 동일 적용 필요.
	bBroken = true;
	bBreakVisualPlayed = true;   // 재연출 방지
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearTimer(BrokenHideTimerHandle); }
	StopHitShake();

	if (IntactMesh)
	{
		IntactMesh->SetVisibility(false, true);
		IntactMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (FracturedMesh)
	{
		FracturedMesh->SetSimulatePhysics(false);
		FracturedMesh->SetVisibility(false, true);
		FracturedMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

void ARetrieveDestructibleResourceActor::ApplyIntactState()
{
	// 로드: 미채굴 슬롯 → 온전 상태로 복원(연출 없이).
	bBroken = false;
	bBreakVisualPlayed = false;
	CurrentHitCount = 0;
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearTimer(BrokenHideTimerHandle); }
	StopHitShake();

	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);

	if (FracturedMesh)
	{
		FracturedMesh->SetSimulatePhysics(false);
		FracturedMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		FracturedMesh->SetVisibility(false, true);
		// 흩어진 조각의 동적 상태 초기화(Rest Collection + Physics State 재생성). 재파괴 연출 정상화.
		// ResetDynamicCollection()은 protected이므로 public ResetState() 사용(UE 5.7).
		FracturedMesh->ResetState();
	}
	if (IntactMesh)
	{
		IntactMesh->SetVisibility(true, true);
		IntactMesh->SetCollisionEnabled(DefaultIntactCollision);
		if (bHitBaseTransformCaptured) { IntactMesh->SetRelativeTransform(HitBaseRelativeTransform); }
	}
	UpdateCrackProgress(0.0f);
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
		// 신선한 파괴 연출(숨김 상태로 복제됐으면 안 보임 → 로드 복원 케이스와 정합).
		ApplyBrokenVisual(GetActorLocation());
	}
	else
	{
		ApplyIntactState();   // 복원(intact) 클라 반영
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
