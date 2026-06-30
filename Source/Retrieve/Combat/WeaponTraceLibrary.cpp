#include "Combat/WeaponTraceLibrary.h"

#include "Components/MeshComponent.h"

namespace
{
	void FillInterpolatedPoints(const FVector& Start, const FVector& End, int32 SegmentCount, TArray<FVector>& OutPoints)
	{
		const int32 Count = FMath::Max(2, SegmentCount);
		OutPoints.Reset();
		OutPoints.Reserve(Count);
		for (int32 i = 0; i < Count; ++i)
		{
			const float Alpha = static_cast<float>(i) / static_cast<float>(Count - 1);
			OutPoints.Add(FMath::Lerp(Start, End, Alpha));
		}
	}
}

bool URetrieveWeaponTraceLibrary::BuildSocketTrace(const UMeshComponent* Mesh, FName StartSocket, FName EndSocket,
	int32 SegmentCount, float Radius, FWeaponTraceSegment& OutSegment)
{
	OutSegment = FWeaponTraceSegment();

	if (!IsValid(Mesh) || Radius <= 0.f)
	{
		return false;
	}
	if (StartSocket.IsNone() || EndSocket.IsNone()
		|| !Mesh->DoesSocketExist(StartSocket) || !Mesh->DoesSocketExist(EndSocket))
	{
		return false;
	}

	const FVector StartLoc = Mesh->GetSocketLocation(StartSocket);
	const FVector EndLoc = Mesh->GetSocketLocation(EndSocket);
	FillInterpolatedPoints(StartLoc, EndLoc, SegmentCount, OutSegment.Points);
	OutSegment.Radius = Radius;
	return true;
}

bool URetrieveWeaponTraceLibrary::BuildBoundsTrace(const UMeshComponent* Mesh, float RadiusScale, float LengthPadding,
	int32 SegmentCount, FWeaponTraceSegment& OutSegment)
{
	OutSegment = FWeaponTraceSegment();

	if (!IsValid(Mesh))
	{
		return false;
	}

	// 로컬 바운드(축 정렬 바운딩박스). Identity로 호출하면 컴포넌트 로컬 공간 기준
	const FBoxSphereBounds LocalBounds = Mesh->CalcBounds(FTransform::Identity);
	const FVector Extent = LocalBounds.BoxExtent;   // 절반 크기
	const FVector Center = LocalBounds.Origin;

	// 최장축을 '날' 방향으로 간주
	int32 LongAxis = 0;
	if (Extent.Y > Extent[LongAxis]) { LongAxis = 1; }
	if (Extent.Z > Extent[LongAxis]) { LongAxis = 2; }

	const float HalfLen = Extent[LongAxis];
	if (HalfLen <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	// 반경 = 나머지 두 축 절반 중 큰 값 × 배율(최소 1)
	float CrossHalf = 0.f;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		if (Axis != LongAxis)
		{
			CrossHalf = FMath::Max(CrossHalf, Extent[Axis]);
		}
	}
	const float Radius = FMath::Max(1.f, CrossHalf * FMath::Max(0.f, RadiusScale));

	FVector AxisDir = FVector::ZeroVector;
	AxisDir[LongAxis] = 1.f;
	const float HalfSpan = HalfLen + LengthPadding;
	const FVector LocalStart = Center - AxisDir * HalfSpan;
	const FVector LocalEnd = Center + AxisDir * HalfSpan;

	const FTransform CompXform = Mesh->GetComponentTransform();
	const FVector WorldStart = CompXform.TransformPosition(LocalStart);
	const FVector WorldEnd = CompXform.TransformPosition(LocalEnd);

	FillInterpolatedPoints(WorldStart, WorldEnd, SegmentCount, OutSegment.Points);
	OutSegment.Radius = Radius;
	return true;
}

bool URetrieveWeaponTraceLibrary::BuildBoundsSphere(const UMeshComponent* Mesh, float RadiusScale, FWeaponTraceSegment& OutSegment)
{
	OutSegment = FWeaponTraceSegment();

	if (!IsValid(Mesh))
	{
		return false;
	}

	const FBoxSphereBounds LocalBounds = Mesh->CalcBounds(FTransform::Identity);
	const FVector Extent = LocalBounds.BoxExtent;

	// 둥근 판(방패)의 면 반지름 ≈ 최대 절반 크기
	const float FaceRadius = FMath::Max3(Extent.X, Extent.Y, Extent.Z);
	if (FaceRadius <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector WorldCenter = Mesh->GetComponentTransform().TransformPosition(LocalBounds.Origin);
	OutSegment.Points.Add(WorldCenter);
	OutSegment.Radius = FMath::Max(1.f, FaceRadius * FMath::Max(0.f, RadiusScale));
	return true;
}
