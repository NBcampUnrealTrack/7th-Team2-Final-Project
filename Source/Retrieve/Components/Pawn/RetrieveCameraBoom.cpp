

#include "RetrieveCameraBoom.h"

#include "AbilitySystemComponent.h"
#include "Character/RetrieveAlsCharacter.h"
#include "DrawDebugHelpers.h"
#include "Components/Pawn/RetrievePawnExtensionComponent.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "HAL/IConsoleManager.h"

namespace
{
	static TAutoConsoleVariable<int32> CVarDebugSpringArm(
		TEXT("Retrieve.Camera.DebugSpringArm"),
		0,
		TEXT("Log SpringArm camera-channel sweep diagnostics. 0=off, 1=on."));

	static TAutoConsoleVariable<int32> CVarDebugSpringArmDraw(
		TEXT("Retrieve.Camera.DebugSpringArmDraw"),
		0,
		TEXT("Draw SpringArm camera-channel sweep diagnostics. 0=off, 1=on."));
}

void URetrieveCameraBoom::AddZoomInput(float AxisValue)
{
	// 락온 중 줌 인/아웃 방지(선택사항)
	// if (IsOwnerLockedOn())
	// {
	// 	return;
	// }
	if (DesiredArmLength < 0.f)
	{
		DesiredArmLength = TargetArmLength;
	}

	const float NewDesired = DesiredArmLength - AxisValue * ZoomStep;
	// 충돌로 당겨진 상태에서 줌아웃은 누적 금지
	if (IsCollisionFixApplied() && NewDesired > DesiredArmLength)
	{
		return;
	}
	DesiredArmLength = FMath::Clamp(NewDesired, MinArmLength, MaxArmLength);
}

void URetrieveCameraBoom::UpdateDesiredArmLocation(bool bDoTrace, bool bDoLocationLag, bool bDoRotationLag,
                                                   float DeltaTime)
{
	// 줌: 목표 거리로 부드럽게
	if (DesiredArmLength >= 0.f)
	{
		TargetArmLength = FMath::FInterpTo(TargetArmLength, DesiredArmLength, DeltaTime, ZoomInterpSpeed);
	}

	// 맨틀 중 충돌 테스트 off: 캡슐이 턱 솔리드를 타고 올라 프로브가 붕괴 → 카메라 허리 관통 방지
	if (const ARetrieveAlsCharacter* OwnerChar = Cast<ARetrieveAlsCharacter>(GetOwner()))
	{
		if (OwnerChar->IsMantling())
		{
			bDoTrace = false;
		}
	}

	Super::UpdateDesiredArmLocation(bDoTrace, bDoLocationLag, bDoRotationLag, DeltaTime);
	// 붐에 붙은 자식 카메라 캐싱
	if (IsValid(ChildCamera) == false)
	{
		const TArray<USceneComponent*>& Kids = GetAttachChildren();
		ChildCamera = Kids.Num() > 0 ? Kids[0] : nullptr;
	}

	if (IsValid(ChildCamera) == false)
	{
		return;
	}

	if (CVarDebugSpringArm.GetValueOnGameThread() != 0)
	{
		const FVector TraceStart = GetComponentLocation() + TargetOffset;
		const FRotator DesiredRot = GetTargetRotation();
		const FVector TraceEnd = TraceStart - DesiredRot.Vector() * TargetArmLength;

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RetrieveCameraSpringArmDebug), false, GetOwner());
		FHitResult Hit;
		const bool bHit = GetWorld() && GetWorld()->SweepSingleByChannel(
			Hit,
			TraceStart,
			TraceEnd,
			FQuat::Identity,
			ProbeChannel,
			FCollisionShape::MakeSphere(ProbeSize),
			QueryParams);

		const FVector CameraLocation = ChildCamera->GetComponentLocation();
		const float CurrentArmDistance = FVector::Distance(TraceStart, CameraLocation);
		UE_LOG(LogTemp, Warning,
			TEXT("[CameraSpringArmDebug] Owner=%s DoTrace=%d ProbeChannel=%d ProbeSize=%.1f TargetArm=%.1f CurrentDist=%.1f Fixed=%d Start=%s End=%s Camera=%s Hit=%d Actor=%s Comp=%s HitLoc=%s Impact=%s Dist=%.1f Blocking=%d StartPen=%d PenDepth=%.1f"),
			*GetNameSafe(GetOwner()),
			bDoTrace ? 1 : 0,
			static_cast<int32>(ProbeChannel),
			ProbeSize,
			TargetArmLength,
			CurrentArmDistance,
			IsCollisionFixApplied() ? 1 : 0,
			*TraceStart.ToCompactString(),
			*TraceEnd.ToCompactString(),
			*CameraLocation.ToCompactString(),
			bHit ? 1 : 0,
			bHit ? *GetNameSafe(Hit.GetActor()) : TEXT("None"),
			bHit ? *GetNameSafe(Hit.GetComponent()) : TEXT("None"),
			bHit ? *Hit.Location.ToCompactString() : TEXT("None"),
			bHit ? *Hit.ImpactPoint.ToCompactString() : TEXT("None"),
			bHit ? Hit.Distance : 0.f,
			bHit && Hit.bBlockingHit ? 1 : 0,
			bHit && Hit.bStartPenetrating ? 1 : 0,
			bHit ? Hit.PenetrationDepth : 0.f);

		if (CVarDebugSpringArmDraw.GetValueOnGameThread() != 0 && GetWorld())
		{
			DrawDebugLine(GetWorld(), TraceStart, TraceEnd, bHit ? FColor::Red : FColor::Yellow, false, 0.f, 0, 1.f);
			DrawDebugSphere(GetWorld(), TraceStart, ProbeSize * 2.f, 12, FColor::Green, false, 0.f);
			DrawDebugSphere(GetWorld(), TraceEnd, ProbeSize * 2.f, 12, FColor::Yellow, false, 0.f);
			DrawDebugSphere(GetWorld(), CameraLocation, ProbeSize * 2.f, 12, FColor::Cyan, false, 0.f);
			if (bHit)
			{
				DrawDebugSphere(GetWorld(), Hit.Location, ProbeSize * 3.f, 16, FColor::Red, false, 0.f);
				DrawDebugPoint(GetWorld(), Hit.ImpactPoint, 12.f, FColor::Magenta, false, 0.f);
			}
		}
	}

	// 락온 중엔 0으로 복귀(리그가 SocketOffset 관리), 아니면 충돌 시 숄더
	const FVector Desired = IsOwnerLockedOn()
		? FVector::ZeroVector
		: (IsCollisionFixApplied() ? ShoulderOffset : BaseOffset);

	// SocketOffset이 아니라 카메라 RelativeLocation에 적용 (프로브에 영향 X)
	ChildCamera->SetRelativeLocation(
		FMath::VInterpTo(ChildCamera->GetRelativeLocation(), Desired, DeltaTime, ShoulderBlendSpeed));

	// 카메라가 강 수면 아래면 Z를 수면 위로 올림 — 물속이 안 보이게.
	// 수면Z는 물 메시에 '아래로 트레이스'해서 직접 읽음(스플라인 키 해석 안 씀 → 메시 연속 → 지터 없음).
	// WaterTraceChannel(WaterCamera)은 강 물 메시만 Block → 바위·지형 등은 통과하고 물만 맞음.
	if (GetWorld())
	{
		const FVector CamWorld = ChildCamera->GetComponentLocation();
		const FVector Start(CamWorld.X, CamWorld.Y, CamWorld.Z + WaterTraceUp);
		const FVector End(CamWorld.X, CamWorld.Y, CamWorld.Z - WaterTraceDown);

		FHitResult Hit;
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RetrieveCameraWaterSurface), false, GetOwner());
		const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, WaterTraceChannel, QueryParams);

		if (bHit)
		{
			LastWaterSurfaceZ = Hit.ImpactPoint.Z;
			WaterMissTime = 0.f;
		}
		else
		{
			WaterMissTime += DeltaTime;
		}

		// 맞았거나, 직전 hit 후 짧은 시간 내 미스(=스플라인 콜리전 빈틈)면 수면으로 간주.
		if (bHit || WaterMissTime <= WaterMissHoldTime)
		{
			const float SurfaceZ = bHit ? Hit.ImpactPoint.Z : LastWaterSurfaceZ;
			const float MinZ = SurfaceZ + WaterSurfaceCameraMargin;
			if (CamWorld.Z < MinZ)
			{
				ChildCamera->SetWorldLocation(FVector(CamWorld.X, CamWorld.Y, MinZ));
			}
		}
	}
}

bool URetrieveCameraBoom::IsOwnerLockedOn() const
{
	const URetrievePawnExtensionComponent* PawnExt =
	   URetrievePawnExtensionComponent::FindPawnExtensionComponent(GetOwner());
	const UAbilitySystemComponent* ASC =
		PawnExt ? PawnExt->GetRetrieveAbilitySystemComponent() : nullptr;
	return ASC && ASC->HasMatchingGameplayTag(RetrieveGameplayTags::LockOn_Active);
}
