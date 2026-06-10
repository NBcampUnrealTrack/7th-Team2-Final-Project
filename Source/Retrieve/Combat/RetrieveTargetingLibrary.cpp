#include "Combat/RetrieveTargetingLibrary.h"

#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GenericTeamAgentInterface.h"

#if ENABLE_DRAW_DEBUG
#include "DrawDebugHelpers.h"
#endif

namespace
{
	TAutoConsoleVariable<int32> CVarWarpDebug(
		TEXT("Retrieve.Warp.Debug"),
		0,
		TEXT("0: off, 1: 오토타게팅 콘/선택 타겟 표시"),
		ECVF_Cheat);
}

AActor* URetrieveTargetingLibrary::FindBestTarget(ACharacter* Source, float Range, float HalfAngle,
	FVector AimDirection, float MaxVerticalDelta, float RangeWeightRate)
{
	if (!IsValid(Source) || Range <= 0.f)
	{
		return nullptr;
	}

	const UWorld* World = Source->GetWorld();
	if (!IsValid(World))
	{
		return nullptr;
	}

	const FVector Origin = Source->GetActorLocation();

	FVector Aim = AimDirection.GetSafeNormal2D();
	if (Aim.IsNearlyZero())
	{
		Aim = Source->GetActorForwardVector().GetSafeNormal2D();
	}

	TArray<FOverlapResult> Overlaps;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(RetrieveWarpAcquire), false, Source);
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel1);
	World->OverlapMultiByObjectType(
		Overlaps,
		Origin,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(Range),
		Params);

	AActor* Result = nullptr;
	float BestScore = -FLT_MAX;

	TSet<const AActor*> Seen;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();
		if (!IsValid(Candidate) || Candidate == Source)
		{
			continue;
		}

		bool bAlready = false;
		Seen.Add(Candidate, &bAlready);
		if (bAlready)
		{
			continue;
		}

		if (FGenericTeamId::GetAttitude(Source, Candidate) != ETeamAttitude::Hostile)
		{
			continue;
		}

		const FVector Delta = Candidate->GetActorLocation() - Origin;
		if (FMath::Abs(Delta.Z) > MaxVerticalDelta)
		{
			continue;   // 높이 차 과대 → 닿을 수 없는 대상 제외(비평지)
		}

		const FVector To2D(Delta.X, Delta.Y, 0.f);
		const float Dist = To2D.Size();
		if (Dist <= KINDA_SMALL_NUMBER || Dist > Range)
		{
			continue;
		}

		const FVector ToDir = To2D / Dist;
		const float AngleDeg = FMath::RadiansToDegrees(
			FMath::Acos(FMath::Clamp(FVector::DotProduct(Aim, ToDir), -1.f, 1.f)));
		if (AngleDeg > HalfAngle)
		{
			continue;
		}

		const float LengthWeight = ((Range - Dist) / Range) * RangeWeightRate;
		const float AngleWeight = ((180.f - AngleDeg) / 180.f) * (1.f - RangeWeightRate);
		const float Score = LengthWeight + AngleWeight;
		if (Score > BestScore)
		{
			BestScore = Score;
			Result = Candidate;
		}
	}

#if ENABLE_DRAW_DEBUG
	if (CVarWarpDebug.GetValueOnGameThread() != 0)
	{
		constexpr float DrawTime = 1.8f;
		const FVector EdgeL = Aim.RotateAngleAxis(HalfAngle, FVector::UpVector) * Range;
		const FVector EdgeR = Aim.RotateAngleAxis(-HalfAngle, FVector::UpVector) * Range;
		DrawDebugLine(World, Origin, Origin + EdgeL, FColor::Yellow, false, DrawTime);
		DrawDebugLine(World, Origin, Origin + EdgeR, FColor::Yellow, false, DrawTime);
		DrawDebugDirectionalArrow(World, Origin, Origin + Aim * Range, 60.f, FColor::Green, false, DrawTime);
		if (Result)
		{
			DrawDebugLine(World, Origin, Result->GetActorLocation(), FColor::Red, false, DrawTime, 0, 2.f);
		}
	}
#endif

	return Result;
}

FTransform URetrieveTargetingLibrary::BuildWarpTransform(ACharacter* Source, AActor* Target, float StandoffOffset, float MaxWarpDistance)
{
	if (!IsValid(Source) || !IsValid(Target))
	{
		return IsValid(Source)
			? FTransform(Source->GetActorRotation(), Source->GetActorLocation())
			: FTransform::Identity;
	}

	const FVector OwnerLocation = Source->GetActorLocation();
	const FVector Delta = Target->GetActorLocation() - OwnerLocation;
	const FVector To2D(Delta.X, Delta.Y, 0.f);
	const float Length = To2D.Size();
	if (Length <= KINDA_SMALL_NUMBER)
	{
		return FTransform(Source->GetActorRotation(), OwnerLocation);
	}

	const FVector ToDir = To2D / Length;

	const float CasterRadius = Source->GetCapsuleComponent()
		? Source->GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.f;
	float TargetRadius = 0.f;
	if (const ACharacter* TargetChar = Cast<ACharacter>(Target))
	{
		TargetRadius = TargetChar->GetCapsuleComponent()->GetScaledCapsuleRadius();
	}
	const float Standoff = CasterRadius + TargetRadius + StandoffOffset;

	// 도약 거리 = min( 타겟까지(− standoff),  MaxWarpDistance(=섹션 루트모션 전진량) )
	// MaxWarpDistance는 항상 상한. 0이면 전진 0(회전만) — in-place 애님과 일관.
	float Travel = (Length <= Standoff) ? 0.f : (Length - Standoff);
	Travel = FMath::Min(Travel, FMath::Max(0.f, MaxWarpDistance));

	FVector WarpLocation = OwnerLocation + ToDir * Travel;
	WarpLocation.Z = OwnerLocation.Z; // 워프 Z 무시

	return FTransform(ToDir.Rotation(), WarpLocation);
}
