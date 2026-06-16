#include "Components/Water/RetrieveRiverWaterProviderComponent.h"

#include "Components/BoxComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/Water/RetrieveCameraWaterProbeComponent.h"
#include "Components/SplineComponent.h"
#include "Components/Water/SwimDetectionComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Settings/RetrieveSwimSettings.h"

URetrieveRiverWaterProviderComponent::URetrieveRiverWaterProviderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URetrieveRiverWaterProviderComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner) { return; }

	Spline = Owner->FindComponentByClass<USplineComponent>();
	if (!Spline) { return; }

	// 스플라인 포인트 바운드 → 폭/깊이 패딩으로 AABB.
	FBox Bounds(ForceInit);
	const int32 Num = Spline->GetNumberOfSplinePoints();
	for (int32 i = 0; i < Num; ++i)
	{
		Bounds += Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
	}
	Bounds = Bounds.ExpandBy(FVector(HalfWidth, HalfWidth, 0.f));
	Bounds.Min.Z -= Depth;
	Bounds.Max.Z += SurfaceMargin;

	WaterBox = NewObject<UBoxComponent>(Owner);
	WaterBox->RegisterComponent();
	WaterBox->AttachToComponent(Owner->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
	WaterBox->SetWorldRotation(FQuat::Identity);
	WaterBox->SetWorldLocation(Bounds.GetCenter());
	WaterBox ->SetBoxExtent(Bounds.GetExtent());
	WaterBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	WaterBox->SetGenerateOverlapEvents(true);
	WaterBox->OnComponentBeginOverlap.AddDynamic(this, &URetrieveRiverWaterProviderComponent::HandleBeginOverlap);
	WaterBox->OnComponentEndOverlap.AddDynamic(this, &URetrieveRiverWaterProviderComponent::HandleEndOverlap);

	// 수중 PP = 호수 TriggerPostProcessVolume→PostProcessing 방식 그대로: 박스 바운드 PostProcessComponent.
	// 이 머티리얼은 unbound로는 안 켜지고(볼륨 바운드 필요), 박스 안에 카메라가 들어와야 적용됨.
	const FRetrieveWaterPPMaterials PPMats = GetWaterPostProcessMaterials_Implementation();
	if (PPMats.Underwater)
	{
		UnderwaterPP = NewObject<UPostProcessComponent>(Owner);
		UnderwaterPP->SetupAttachment(WaterBox);
		UnderwaterPP->RegisterComponent();
		UnderwaterPP->bUnbound = false;
		UnderwaterPP->bUseAttachParentBound = true; // WaterBox 바운드 사용(원형 PostProcessing 동일)
		UnderwaterPP->BlendWeight = 1.f;

		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(PPMats.Underwater, this);
		MID->SetScalarParameterValue(TEXT("[BP] Deep Water Fog Start"), Bounds.Max.Z - SurfaceMargin); // 박스 윗면-Margin ≈ 수면
		UnderwaterPP->Settings.AddBlendable(MID, 1.f);
	}
}

void URetrieveRiverWaterProviderComponent::HandleBeginOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32, bool, const FHitResult&)
{
	if (OtherComp && OtherComp->IsA<URetrieveCameraWaterProbeComponent>())
	{
		return;
	}

	if (USwimDetectionComponent* Swim =
		OtherActor ? OtherActor->FindComponentByClass<USwimDetectionComponent>() : nullptr)
	{
		Swim->NotifyEnterWaterRegion(this);
	}
}

void URetrieveRiverWaterProviderComponent::HandleEndOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32)
{
	if (OtherComp && OtherComp->IsA<URetrieveCameraWaterProbeComponent>())
	{
		return;
	}

	if (USwimDetectionComponent* Swim =
		OtherActor ? OtherActor->FindComponentByClass<USwimDetectionComponent>() : nullptr)
	{
		Swim->NotifyExitWaterRegion(this);
	}
}

int32 URetrieveRiverWaterProviderComponent::ReadBiomeIndex() const
{
	// 강 BP가 이미 들고있는 바이옴 값 재사용
	if (const AActor* Owner = GetOwner())
	{
		if (const FByteProperty* BiomeProp = FindFProperty<FByteProperty>(Owner->GetClass(), TEXT("River Material Presets")))
		{
			return BiomeProp->GetPropertyValue_InContainer(Owner);
		}
	}
	return 0; // 커스텀 폴백
}

float URetrieveRiverWaterProviderComponent::GetWaterSurfaceZ_Implementation(const FVector& Location) const
{
	if (!Spline) { return Location.Z; }
	const float Key = Spline->FindInputKeyClosestToWorldLocation(Location);
	return Spline->GetLocationAtSplineInputKey(Key, ESplineCoordinateSpace::World).Z + SurfaceZOffset;
}

bool URetrieveRiverWaterProviderComponent::TryGetWaterColumn_Implementation(const FVector& Location, float& OutSurfaceZ) const
{
	if (!Spline) { return false; }

	const float Key = Spline->FindInputKeyClosestToWorldLocation(Location);
	const FTransform T = Spline->GetTransformAtSplineInputKey(Key, ESplineCoordinateSpace::World);
	const FVector Off = Location - T.GetLocation();

	// 스플라인 로컬 Y(폭) / Z(깊이) 축으로 채널 판정.
	// 깊이 초과는 이탈 사유 아님(바닥=floor cap, 진짜 이탈=박스 바텀 오버랩). 가로/수면위만 채널 밖.
	const float Lateral = FMath::Abs(FVector::DotProduct(Off, T.GetUnitAxis(EAxis::Y)));
	const float Vertical = FVector::DotProduct(Off, T.GetUnitAxis(EAxis::Z));
	if (Lateral > HalfWidth || Vertical > SurfaceMargin)
	{
		return false;
	}
	OutSurfaceZ = T.GetLocation().Z + SurfaceZOffset;
	return true;
}

FVector URetrieveRiverWaterProviderComponent::GetFlowVelocity_Implementation(const FVector& Location) const
{
	if (!Spline) { return FVector::ZeroVector; }
	const float Key = Spline->FindInputKeyClosestToWorldLocation(Location);
	return Spline->GetDirectionAtSplineInputKey(Key, ESplineCoordinateSpace::World) * FlowStrength;
}

FRetrieveWaterPPMaterials URetrieveRiverWaterProviderComponent::GetWaterPostProcessMaterials_Implementation() const
{
	FRetrieveWaterPPMaterials Out;
	const URetrieveSwimSettings* Settings = GetDefault<URetrieveSwimSettings>();
	if (!Settings) { return Out; }
	
	const int32  Biome = (BiomeOverride >= 0) ? BiomeOverride : ReadBiomeIndex();
	if (Settings->UnderwaterByBiome.IsValidIndex(Biome))
	{
		Out.Underwater = Settings->UnderwaterByBiome[Biome].LoadSynchronous();
	}
	Out.Waterline = Settings->WaterlineMaterial.LoadSynchronous();

	return Out;
}
