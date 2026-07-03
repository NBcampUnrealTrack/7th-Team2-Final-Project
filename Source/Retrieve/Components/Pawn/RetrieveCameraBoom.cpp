

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

void URetrieveCameraBoom::SetCameraBoomProfileOverride(FName OverrideId, const FRetrieveCameraBoomProfile& Profile)
{
	// None ID는 해제 시 소유권을 판별할 수 없으므로 허용하지 않는다.
	if (OverrideId.IsNone())
	{
		return;
	}

	// 첫 override 진입 직전 구도를 저장한다.
	// BowAim 같은 일시 구도가 끝났을 때, 플레이어가 맞춰둔 줌/기본 숄더 상태로 돌아가기 위함.
	CacheReturnCameraProfileIfNeeded();

	bHasCameraProfileOverride = true;
	ActiveCameraProfileOverrideId = OverrideId;
	ActiveCameraProfileOverride = Profile;
}

void URetrieveCameraBoom::ClearCameraBoomProfileOverride(FName OverrideId)
{
	// 현재 활성 override를 건 쪽만 해제할 수 있다.
	// 늦게 끝난 다른 어빌리티가 새로 올라온 카메라 구도를 지우는 상황을 막는다.
	if (!bHasCameraProfileOverride || ActiveCameraProfileOverrideId != OverrideId)
	{
		return;
	}

	bHasCameraProfileOverride = false;
	ActiveCameraProfileOverrideId = NAME_None;
}

void URetrieveCameraBoom::UpdateDesiredArmLocation(bool bDoTrace, bool bDoLocationLag, bool bDoRotationLag,
                                                   float DeltaTime)
{
	// 줌/프로필: 목표 거리로 부드럽게.
	// 외부 프로필이 없을 때는 기존 DesiredArmLength 기반 줌 규칙을 그대로 따른다.
	if (bHasCameraProfileOverride || bHasReturnCameraProfile || DesiredArmLength >= 0.f)
	{
		TargetArmLength = FMath::FInterpTo(
			TargetArmLength,
			GetResolvedArmTargetLength(),
			DeltaTime,
			GetResolvedArmBlendSpeed());
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

	// 락온 중엔 0으로 복귀(리그가 SocketOffset 관리), 아니면 프로필/충돌 상태에 맞는 오프셋으로 이동.
	const FVector Desired = GetResolvedCameraRelativeOffset();

	// SocketOffset이 아니라 카메라 RelativeLocation에 적용 (프로브에 영향 X)
	ChildCamera->SetRelativeLocation(
		FMath::VInterpTo(ChildCamera->GetRelativeLocation(), Desired, DeltaTime, GetResolvedCameraOffsetBlendSpeed()));

	ClearReturnCameraProfileIfRestored();

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

void URetrieveCameraBoom::CacheReturnCameraProfileIfNeeded()
{
	// override가 갱신될 때마다 복귀 지점을 덮어쓰면,
	// "최초 진입 전 구도"가 아니라 "override 중간 구도"로 돌아가게 된다.
	if (bHasReturnCameraProfile)
	{
		return;
	}

	// TargetArmLength는 충돌 보정으로 순간적으로 짧아질 수 있다.
	// 복귀 기준은 실제 카메라 위치가 아니라 플레이어가 의도한 목표 거리로 저장한다.
	ReturnCameraProfile.TargetArmLength = GetCurrentDesiredArmLength();
	ReturnCameraProfile.CameraRelativeOffset = GetDefaultCameraRelativeOffset();
	ReturnCameraProfile.ArmBlendSpeed = ZoomInterpSpeed;
	ReturnCameraProfile.OffsetBlendSpeed = ShoulderBlendSpeed;
	bHasReturnCameraProfile = true;
}

float URetrieveCameraBoom::GetCurrentDesiredArmLength() const
{
	// 줌 입력을 한 번이라도 받은 뒤에는 DesiredArmLength가 사용자의 목표 거리다.
	// 아직 줌 입력이 없다면 현재 TargetArmLength를 기준 구도로 본다.
	return DesiredArmLength >= 0.f ? DesiredArmLength : TargetArmLength;
}

FVector URetrieveCameraBoom::GetDefaultCameraRelativeOffset() const
{
	// 기존 기본 규칙을 한 곳에 모아 둔다.
	// 락온은 중앙 시점, 일반 상태는 충돌 보정 여부에 따라 Base/Shoulder를 선택한다.
	return IsOwnerLockedOn()
		? FVector::ZeroVector
		: (IsCollisionFixApplied() ? ShoulderOffset : BaseOffset);
}

float URetrieveCameraBoom::GetResolvedArmTargetLength() const
{
	// 거리 우선순위: 외부 override > override 해제 후 복귀 > 일반 줌.
	// 락온은 현재 거리에는 관여하지 않고 카메라 상대 오프셋만 중앙으로 되돌린다.
	if (bHasCameraProfileOverride)
	{
		return ActiveCameraProfileOverride.TargetArmLength;
	}

	if (bHasReturnCameraProfile)
	{
		return ReturnCameraProfile.TargetArmLength;
	}

	return GetCurrentDesiredArmLength();
}

float URetrieveCameraBoom::GetResolvedArmBlendSpeed() const
{
	// 목표 거리와 동일한 소스의 보간 속도를 사용해야,
	// BowAim 진입/복귀처럼 의도된 카메라 템포가 유지된다.
	if (bHasCameraProfileOverride)
	{
		return ActiveCameraProfileOverride.ArmBlendSpeed;
	}

	if (bHasReturnCameraProfile)
	{
		return ReturnCameraProfile.ArmBlendSpeed;
	}

	return ZoomInterpSpeed;
}

FVector URetrieveCameraBoom::GetResolvedCameraRelativeOffset() const
{
	// 오프셋 우선순위는 락온이 최상위다.
	// BowAim 도중 락온이 켜져도 기존 락온 카메라 규칙을 보존한다.
	if (IsOwnerLockedOn())
	{
		return FVector::ZeroVector;
	}

	// SpringArm SocketOffset 대신 자식 카메라 RelativeLocation을 바꾼다.
	// 프로브 충돌 계산은 그대로 두고, 화면 구도만 이동시키기 위함.
	if (bHasCameraProfileOverride)
	{
		return ActiveCameraProfileOverride.CameraRelativeOffset;
	}

	if (bHasReturnCameraProfile)
	{
		return ReturnCameraProfile.CameraRelativeOffset;
	}

	return GetDefaultCameraRelativeOffset();
}

float URetrieveCameraBoom::GetResolvedCameraOffsetBlendSpeed() const
{
	// 락온 중에는 기존 ShoulderBlendSpeed를 사용한다.
	// 락온 구도의 속도까지 BowAim 프로필에 끌려가면 전투 카메라 감각이 바뀐다.
	if (IsOwnerLockedOn())
	{
		return ShoulderBlendSpeed;
	}

	if (bHasCameraProfileOverride)
	{
		return ActiveCameraProfileOverride.OffsetBlendSpeed;
	}

	if (bHasReturnCameraProfile)
	{
		return ReturnCameraProfile.OffsetBlendSpeed;
	}

	return ShoulderBlendSpeed;
}

void URetrieveCameraBoom::ClearReturnCameraProfileIfRestored()
{
	// override가 살아있는 동안에는 복귀 프로필을 유지한다.
	// 실제 해제 후 목표 구도에 도달했을 때만 일반 줌/숄더 규칙으로 완전히 넘긴다.
	if (!bHasReturnCameraProfile || bHasCameraProfileOverride)
	{
		return;
	}

	// 보간이 거의 끝나면 복귀 상태를 제거한다.
	// 제거 후에는 충돌 숄더, 플레이어 줌 입력, 락온 변화가 기존 경로로 다시 처리된다.
	const bool bArmRestored = FMath::IsNearlyEqual(TargetArmLength, ReturnCameraProfile.TargetArmLength, 0.5f);
	const bool bOffsetRestored = !IsValid(ChildCamera) ||
		ChildCamera->GetRelativeLocation().Equals(ReturnCameraProfile.CameraRelativeOffset, 0.5f);
	if (!bArmRestored || !bOffsetRestored)
	{
		return;
	}

	DesiredArmLength = ReturnCameraProfile.TargetArmLength;
	bHasReturnCameraProfile = false;
}
