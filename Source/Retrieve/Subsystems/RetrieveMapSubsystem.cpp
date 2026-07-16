#include "Subsystems/RetrieveMapSubsystem.h"
#include "Components/World/RetrieveMapIconComponent.h"
#include "Data/RetrieveMapConfigDataAsset.h"
#include "World/RetrieveIndoorMapCaptureActor.h"
#include "World/RetrieveMinimapAreaVolume.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "RenderUtils.h"
#include "UObject/UObjectIterator.h"

void URetrieveMapSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	BoundsRetryAttempts = 0;
	if (!InitializeBounds(InWorld))
	{
		InWorld.GetTimerManager().SetTimer(
			BoundsRetryTimerHandle,
			this,
			&URetrieveMapSubsystem::RetryInitializeBounds,
			1.0f,
			true);
	}
}

void URetrieveMapSubsystem::EnsureMapConfigLoaded()
{
	if (MapConfig && HasValidBounds() && BakedMapTexture)
	{
		return;
	}

	InitializeMapConfigRuntime(nullptr);
}

bool URetrieveMapSubsystem::InitializeMapConfigRuntime(URetrieveMapConfigDataAsset* OverrideConfig)
{
	if (OverrideConfig)
	{
		MapConfig = OverrideConfig;
	}

	if (!MapConfig)
	{
		MapConfig = LoadObject<URetrieveMapConfigDataAsset>(
			nullptr,
			TEXT("/Game/Retrieve/Data/Map/DA_MapConfig.DA_MapConfig")
		);
	}

	if (!MapConfig)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Retrieve|Map] InitializeMapConfigRuntime 실패: MapConfig 없음"));
		return false;
	}

	BakedMapTexture = MapConfig->BakedMapTexture;
	MapOrigin       = MapConfig->MapOrigin;
	MapExtentXY     = MapConfig->MapExtentXY;
	MapExtent       = FMath::Max(MapExtentXY.X, MapExtentXY.Y);

	UE_LOG(LogTemp, Log,
		TEXT("[Retrieve|Map] Runtime MapConfig 적용 완료: Origin=(%.0f, %.0f), ExtXY=(%.0f x %.0f), Texture=%s"),
		MapOrigin.X,
		MapOrigin.Y,
		MapExtentXY.X,
		MapExtentXY.Y,
		*GetNameSafe(BakedMapTexture));

	return HasValidBounds();
}

bool URetrieveMapSubsystem::InitializeBounds(UWorld& InWorld, bool bLogWarnings)
{
	AActor* BestCaptureOwner = nullptr;
	USceneCaptureComponent2D* BestComp = nullptr;
	int32 BestScore = MIN_int32;
	int32 CandidateCount = 0;
	int32 OrthographicCount = 0;
	bool bBestFromDifferentWorld = false;

	// ---------------------------------------------------------------------
	// MapConfig 우선 사용
	//
	// DA_MapConfig에 저장된 값을 SceneCapture2D 검색이나 Landscape bounds
	// 계산보다 먼저 사용한다. 로드/적용은 EnsureMapConfigLoaded로 일원화.
	//   - 월드맵 / 미니맵 / 나침반이 같은 좌표계를 사용하고
	//   - SceneCapture 액터 로딩 순서에 영향받지 않으며
	//   - 맵 구조가 바뀌어도 RefreshMapAll() 결과만 반영하면 된다.
	// ---------------------------------------------------------------------
	EnsureMapConfigLoaded();
	if (MapConfig)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(BoundsRetryTimerHandle);
		}
		return true;
	}

	auto ScoreCaptureComponent = [](const USceneCaptureComponent2D* Comp, const AActor* Owner)
	{
		int32 Score = FMath::RoundToInt(FMath::Max(Comp ? Comp->OrthoWidth : 1.0f, 1.0f));
		const FString CaptureName = GetNameSafe(Owner);
		const FString TargetName = GetNameSafe(Comp ? Comp->TextureTarget.Get() : nullptr);
		if (CaptureName.Contains(TEXT("Map")) ||
			CaptureName.Contains(TEXT("Minimap")) ||
			CaptureName.Contains(TEXT("WorldMap")))
		{
			Score += 1000000;
		}
		if (TargetName.Contains(TEXT("Map")) ||
			TargetName.Contains(TEXT("Minimap")) ||
			TargetName.Contains(TEXT("WorldMap")) ||
			TargetName.Contains(TEXT("RT_Map")))
		{
			Score += 2000000;
		}
		return Score;
	};

	for (TObjectIterator<USceneCaptureComponent2D> It; It; ++It)
	{
		USceneCaptureComponent2D* Comp = *It;
		if (!Comp || Comp->GetWorld() != &InWorld)
		{
			continue;
		}

		AActor* Owner = Comp->GetOwner();
		if (!Owner)
		{
			UE_LOG(LogTemp, Verbose,
				TEXT("[Retrieve|Minimap] SceneCaptureComponent2D skipped: %s has no owner"),
				*GetNameSafe(Comp));
			continue;
		}

		++CandidateCount;
		if (Comp->ProjectionType != ECameraProjectionMode::Orthographic)
		{
			UE_LOG(LogTemp, Verbose,
				TEXT("[Retrieve|Minimap] SceneCapture2D skipped: %s is not Orthographic"),
				*GetNameSafe(Owner));
			continue;
		}
		++OrthographicCount;

		const int32 Score = ScoreCaptureComponent(Comp, Owner);

		if (!BestComp || Score > BestScore)
		{
			BestCaptureOwner = Owner;
			BestComp = Comp;
			BestScore = Score;
			bBestFromDifferentWorld = false;
		}
	}

	if (!BestComp)
	{
		int32 OtherWorldCandidateCount = 0;
		int32 OtherWorldOrthographicCount = 0;
		const FString RuntimeWorldName = InWorld.GetName();

		for (TObjectIterator<USceneCaptureComponent2D> It; It; ++It)
		{
			USceneCaptureComponent2D* Comp = *It;
			if (!Comp || Comp->GetWorld() == &InWorld)
			{
				continue;
			}

			AActor* Owner = Comp->GetOwner();
			UWorld* OwnerWorld = Comp->GetWorld();
			if (!Owner || !OwnerWorld)
			{
				continue;
			}

			const FString OwnerWorldName = OwnerWorld->GetName();
			if (!OwnerWorldName.Contains(RuntimeWorldName) && !RuntimeWorldName.Contains(OwnerWorldName))
			{
				continue;
			}

			++OtherWorldCandidateCount;
			if (Comp->ProjectionType != ECameraProjectionMode::Orthographic)
			{
				continue;
			}
			++OtherWorldOrthographicCount;

			const int32 Score = ScoreCaptureComponent(Comp, Owner);
			if (Score < 1000000)
			{
				continue;
			}

			if (!BestComp || Score > BestScore)
			{
				BestCaptureOwner = Owner;
				BestComp = Comp;
				BestScore = Score;
				bBestFromDifferentWorld = true;
			}
		}

		if (bLogWarnings && !BestComp && OtherWorldCandidateCount > 0)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Retrieve|Minimap] Found SceneCaptureComponents only outside runtime world '%s' (Candidates=%d, Orthographic=%d)."),
				*RuntimeWorldName, OtherWorldCandidateCount, OtherWorldOrthographicCount);
		}
	}

	if (BestComp && BestCaptureOwner)
	{
		const float Width = FMath::Max(BestComp->OrthoWidth, 1.0f);
		const FVector Location = BestCaptureOwner->GetActorLocation();

		MapOrigin = FVector2D(Location.X - Width * 0.5f, Location.Y - Width * 0.5f);
		MapExtentXY = FVector2D(Width, Width);
		MapExtent = Width;

		UE_LOG(LogTemp, Log,
			TEXT("[Retrieve|Minimap] SceneCapture2D bounds: Capture=%s Source=%s Origin=(%.0f,%.0f) Extent=%.0f"),
			*GetNameSafe(BestCaptureOwner),
			bBestFromDifferentWorld ? TEXT("EditorWorldFallback") : TEXT("RuntimeWorld"),
			MapOrigin.X, MapOrigin.Y, MapExtent);

		if (BakedMapTexture && BestComp->bCaptureEveryFrame)
		{
			BestComp->bCaptureEveryFrame = false;
			UE_LOG(LogTemp, Log,
				TEXT("[Retrieve|Minimap] SceneCapture2D '%s' capture every frame disabled for baked texture mode"),
				*GetNameSafe(BestCaptureOwner));
		}

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(BoundsRetryTimerHandle);
		}
		return true;
	}

	if (bLogWarnings)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Retrieve|Minimap] No orthographic SceneCapture2D found in world '%s' (SceneCaptureComponents=%d, Orthographic=%d). Falling back to Landscape bounds."),
			*GetNameSafe(&InWorld), CandidateCount, OrthographicCount);
	}

	for (TActorIterator<ALandscape> It(&InWorld); It; ++It)
	{
		const ALandscape* Landscape = *It;
		if (!Landscape)
		{
			continue;
		}

		const FBox Bounds = Landscape->GetComponentsBoundingBox(true);
		if (!Bounds.IsValid)
		{
			if (bLogWarnings)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[Retrieve|Minimap] Landscape bounds skipped: %s returned invalid bounds"),
					*GetNameSafe(Landscape));
			}
			continue;
		}

		const float Width = FMath::Max(Bounds.Max.X - Bounds.Min.X, 1.0f);
		const float Height = FMath::Max(Bounds.Max.Y - Bounds.Min.Y, 1.0f);

		MapExtentXY = FVector2D(Width, Height);
		MapExtent = FMath::Max(Width, Height);
		MapOrigin = FVector2D(Bounds.Min.X, Bounds.Min.Y);

		UE_LOG(LogTemp, Log,
			TEXT("[Retrieve|Minimap] Landscape bounds: Origin=(%.0f,%.0f) ExtXY=(%.0f x %.0f)"),
			MapOrigin.X, MapOrigin.Y, Width, Height);

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(BoundsRetryTimerHandle);
		}
		return true;
	}

	if (bLogWarnings)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Retrieve|Minimap] Failed to initialize map bounds in world '%s'."),
			*GetNameSafe(&InWorld));
	}
	return false;
}

void URetrieveMapSubsystem::RetryInitializeBounds()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	++BoundsRetryAttempts;
	if (InitializeBounds(*World, BoundsRetryAttempts == 1 || BoundsRetryAttempts % 5 == 0))
	{
		return;
	}

	if (BoundsRetryAttempts >= 30)
	{
		World->GetTimerManager().ClearTimer(BoundsRetryTimerHandle);
		UE_LOG(LogTemp, Warning,
			TEXT("[Retrieve|Minimap] Map bounds retry expired. If SceneCapture2D is placed in a World Partition level, disable 'Is Spatially Loaded' or put it in an always-loaded data layer."));
	}
}

void URetrieveMapSubsystem::SetBakedMapTexture(UTexture2D* InTexture)
{
	BakedMapTexture = InTexture;
}

void URetrieveMapSubsystem::SetMapBounds(FVector2D InOrigin, float InExtent)
{
	MapOrigin = InOrigin;
	MapExtent = FMath::Max(InExtent, 1.0f);
	MapExtentXY = FVector2D(MapExtent, MapExtent);
}

void URetrieveMapSubsystem::SetMapBoundsXY(FVector2D InOrigin, float InExtentX, float InExtentY)
{
	MapOrigin = InOrigin;
	MapExtentXY = FVector2D(FMath::Max(InExtentX, 1.0f), FMath::Max(InExtentY, 1.0f));
	MapExtent = FMath::Max(MapExtentXY.X, MapExtentXY.Y);
}

FVector2D URetrieveMapSubsystem::WorldToUV(const FVector& WorldLocation) const
{
	const float SafeExtX = FMath::Max(MapExtentXY.X, 1.0f);
	const float SafeExtY = FMath::Max(MapExtentXY.Y, 1.0f);
	const float U = (WorldLocation.Y - MapOrigin.Y) / SafeExtY;
	const float V = 1.0f - (WorldLocation.X - MapOrigin.X) / SafeExtX;
	return FVector2D(U, V);
}

FVector URetrieveMapSubsystem::UVToWorld(const FVector2D& UV) const
{
	const float SafeExtX = FMath::Max(MapExtentXY.X, 1.0f);
	const float SafeExtY = FMath::Max(MapExtentXY.Y, 1.0f);
	// WorldToUV 역함수:
	//   U = (WorldY - OriginY) / ExtY  →  WorldY = U * ExtY + OriginY
	//   V = 1 - (WorldX - OriginX) / ExtX  →  WorldX = (1-V) * ExtX + OriginX
	const float WorldX = (1.0f - UV.Y) * SafeExtX + MapOrigin.X;
	const float WorldY = UV.X * SafeExtY + MapOrigin.Y;
	return FVector(WorldX, WorldY, 0.0f);
}

int32 URetrieveMapSubsystem::AddUserWaypoint(FVector InWorldLocation, FText InLabel, FLinearColor InColor)
{
	FUserWaypoint NewWP;
	NewWP.WaypointId    = NextWaypointId++;
	NewWP.WorldLocation = InWorldLocation;
	NewWP.Color         = InColor;

	if (InLabel.IsEmpty())
	{
		NewWP.Label = FText::FromString(FString::Printf(TEXT("[%d]"), NewWP.WaypointId + 1));
	}
	else
	{
		NewWP.Label = InLabel;
	}

	UserWaypoints.Add(NewWP);
	return NewWP.WaypointId;
}

bool URetrieveMapSubsystem::RemoveUserWaypointById(int32 InWaypointId)
{
	const int32 Removed = UserWaypoints.RemoveAll(
		[InWaypointId](const FUserWaypoint& WP){ return WP.WaypointId == InWaypointId; });
	return Removed > 0;
}

void URetrieveMapSubsystem::ClearUserWaypoints()
{
	UserWaypoints.Empty();
}

float URetrieveMapSubsystem::GetZoom(float ViewWorldRadius) const
{
	const float SafeRadius = FMath::Max(ViewWorldRadius, 1.0f);
	const float ShortExtent = FMath::Min(
		FMath::Max(MapExtentXY.X, 1.0f),
		FMath::Max(MapExtentXY.Y, 1.0f)
	);
	return ShortExtent / (SafeRadius * 2.0f);
}

void URetrieveMapSubsystem::DebugPrintBounds() const
{
	UE_LOG(LogTemp, Warning,
		TEXT("[Retrieve|Minimap] MapOrigin=(%.1f, %.1f) ExtentXY=(%.1f x %.1f) MapExtent=%.1f"),
		MapOrigin.X, MapOrigin.Y,
		MapExtentXY.X, MapExtentXY.Y,
		MapExtent
	);

#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1, 8.0f, FColor::Yellow,
			FString::Printf(
				TEXT("[Retrieve|Minimap] Origin=(%.0f,%.0f) ExtXY=(%.0f x %.0f) Extent=%.0f"),
				MapOrigin.X, MapOrigin.Y,
				MapExtentXY.X, MapExtentXY.Y,
				MapExtent
			)
		);
	}
#endif
}

UTexture2D* URetrieveMapSubsystem::GetTextureForUV(const FVector2D& UV) const
{
	for (const FRetrieveMapTile& Tile : MapTiles)
	{
		if (Tile.HasTexture() &&
		    UV.X >= Tile.UVMin.X && UV.X <= Tile.UVMax.X &&
		    UV.Y >= Tile.UVMin.Y && UV.Y <= Tile.UVMax.Y)
		{
			return Tile.Texture;
		}
	}

	return BakedMapTexture;
}

void URetrieveMapSubsystem::RefreshWorldMapSnapshots()
{
	UWorld* World = GetWorld();
	if (!World) { return; }

	int32 Found = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor)) { continue; }

		URetrieveMapIconComponent* IconComp =
			Actor->FindComponentByClass<URetrieveMapIconComponent>();
		if (!IsValid(IconComp)) { continue; }

		RegisterIcon(IconComp);
		++Found;
	}

	UE_LOG(LogTemp, Log,
		TEXT("[WorldMap] RefreshWorldMapSnapshots — 스캔 완료: 로드된 아이콘 %d개, 스냅샷 총 %d개"),
		Found, IconSnapshots.Num());
}

void URetrieveMapSubsystem::DebugPrintIconSnapshots() const
{
	UE_LOG(LogTemp, Warning,
		TEXT("[WorldMap] ─── IconSnapshot 목록 (총 %d개) ───"), IconSnapshots.Num());

	for (int32 i = 0; i < IconSnapshots.Num(); ++i)
	{
		const FRetrieveMapIconSnapshot& S = IconSnapshots[i];
		UE_LOG(LogTemp, Warning,
			TEXT("[WorldMap]  [%d] Type=%d Loc=(%.0f,%.0f,%.0f) Label=%s"),
			i,
			static_cast<int32>(S.IconType),
			S.WorldLocation.X, S.WorldLocation.Y, S.WorldLocation.Z,
			*S.MapLabel.ToString());
	}

#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Cyan,
			FString::Printf(TEXT("[WorldMap] IconSnapshots: %d개"), IconSnapshots.Num()));
	}
#endif
}

void URetrieveMapSubsystem::RegisterIcon(URetrieveMapIconComponent* Icon)
{
	if (!IsValid(Icon) || !IsValid(Icon->GetOwner())) { return; }

	Icons.AddUnique(Icon);

	// 스냅샷 upsert: 이미 등록된 액터면 위치만 갱신, 처음이면 새로 추가
	const FObjectKey Key(Icon->GetOwner());
	if (const int32* Idx = SnapshotIndexByActor.Find(Key))
	{
		IconSnapshots[*Idx].WorldLocation = Icon->GetOwner()->GetActorLocation();
	}
	else
	{
		FRetrieveMapIconSnapshot Snap;
		Snap.WorldLocation      = Icon->GetOwner()->GetActorLocation();
		Snap.IconType           = Icon->IconType;
		Snap.MapLabel           = Icon->MapLabel;
		Snap.bShowLabelOnWorldMap = Icon->bShowLabelOnWorldMap;
		Snap.bOverrideIcon      = Icon->bOverrideIcon;
		Snap.OverrideTexture    = Icon->OverrideTexture;
		Snap.OverrideColor      = Icon->OverrideColor;
		Snap.OverrideSize       = Icon->OverrideSize;

		const int32 NewIdx = IconSnapshots.Add(Snap);
		SnapshotIndexByActor.Add(Key, NewIdx);
	}
}

void URetrieveMapSubsystem::UnregisterIcon(URetrieveMapIconComponent* Icon)
{
	Icons.Remove(Icon);
	// 스냅샷은 제거하지 않음 — 언로드 후에도 월드맵에 아이콘이 유지되어야 함
}

void URetrieveMapSubsystem::RegisterMinimapArea(ARetrieveMinimapAreaVolume* Area)
{
	if (IsValid(Area))
	{
		MinimapAreas.AddUnique(Area);
	}
}

void URetrieveMapSubsystem::UnregisterMinimapArea(ARetrieveMinimapAreaVolume* Area)
{
	MinimapAreas.Remove(Area);
}

FRetrieveMinimapContext URetrieveMapSubsystem::ResolveMinimapContext(
	const FVector& WorldLocation,
	float OutdoorViewWorldRadius) const
{
	ARetrieveMinimapAreaVolume* BestArea = nullptr;
	int32 BestPriority = MIN_int32;
	float BestVolume = BIG_NUMBER;

	for (ARetrieveMinimapAreaVolume* Area : MinimapAreas)
	{
		if (!IsValid(Area) || !Area->ContainsLocation(WorldLocation))
		{
			continue;
		}

		const FBox AreaBox = Area->GetAreaBox();
		const FVector Size = AreaBox.IsValid ? AreaBox.GetSize() : FVector(BIG_NUMBER);
		const float Volume = Size.X * Size.Y * Size.Z;
		if (!BestArea || Area->Priority > BestPriority ||
			(Area->Priority == BestPriority && Volume < BestVolume))
		{
			BestArea = Area;
			BestPriority = Area->Priority;
			BestVolume = Volume;
		}
	}

	if (BestArea)
	{
		return BestArea->BuildContext();
	}

	FRetrieveMinimapContext OutdoorContext;
	OutdoorContext.DisplayMode = ERetrieveMinimapDisplayMode::WorldMap;
	OutdoorContext.Texture = BakedMapTexture;
	OutdoorContext.MapExtentXY = FVector2D(
		FMath::Max(MapExtentXY.X, 1.0f),
		FMath::Max(MapExtentXY.Y, 1.0f));
	OutdoorContext.MapCenter = MapOrigin + OutdoorContext.MapExtentXY * 0.5f;
	OutdoorContext.ViewWorldRadius = FMath::Max(OutdoorViewWorldRadius, 1.0f);
	return OutdoorContext;
}

void URetrieveMapSubsystem::RequestIndoorCapture(ARetrieveMinimapAreaVolume* Area)
{
	if (!IsValid(Area))
	{
		return;
	}

	if (!IsValid(IndoorCaptureActor))
	{
		if (UWorld* World = GetWorld())
		{
			for (TActorIterator<ARetrieveIndoorMapCaptureActor> It(World); It; ++It)
			{
				IndoorCaptureActor = *It;
				break;
			}
		}
	}

	if (IsValid(IndoorCaptureActor))
	{
		IndoorCaptureActor->RequestCapture(Area);
	}
}

// ── 전장의 안개 (탐색 마스크) ────────────────────────────────────────────────

void URetrieveMapSubsystem::EnsureRevealMask()
{
	const int32 Res = FMath::Max(RevealResolution, 8);
	RevealResolution = Res;

	if (RevealMask.Num() != Res * Res)
	{
		RevealMask.Init(0, Res * Res);
		bRevealMaskDirty = true;
	}

	if (!RevealMaskTexture)
	{
		RevealMaskTexture = UTexture2D::CreateTransient(Res, Res, PF_B8G8R8A8);
		if (RevealMaskTexture)
		{
			RevealMaskTexture->SRGB = false;
			RevealMaskTexture->Filter = TF_Bilinear;
			RevealMaskTexture->AddressX = TA_Clamp;
			RevealMaskTexture->AddressY = TA_Clamp;
			RevealMaskTexture->NeverStream = true;
			RevealMaskTexture->UpdateResource();
			bRevealMaskDirty = true;
		}
	}
}

bool URetrieveMapSubsystem::MarkExploredAtWorld(const FVector& WorldLocation)
{
	if (!HasValidBounds())
	{
		return false;
	}
	EnsureRevealMask();

	const int32 Res = RevealResolution;
	if (RevealMask.Num() != Res * Res)
	{
		return false;
	}

	const FVector2D UV = WorldToUV(WorldLocation);
	const float CxF = UV.X * Res;
	const float CyF = UV.Y * Res;

	// U축(열)=East-West=MapExtentXY.Y, V축(행)=North-South=MapExtentXY.X
	const float ExtX = FMath::Max(MapExtentXY.X, 1.0f);
	const float ExtY = FMath::Max(MapExtentXY.Y, 1.0f);
	const float rCellU = (ExploreWorldRadius / ExtY) * Res;
	const float rCellV = (ExploreWorldRadius / ExtX) * Res;
	if (rCellU < KINDA_SMALL_NUMBER || rCellV < KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const int32 MinX = FMath::Clamp(FMath::FloorToInt(CxF - rCellU), 0, Res - 1);
	const int32 MaxX = FMath::Clamp(FMath::CeilToInt (CxF + rCellU), 0, Res - 1);
	const int32 MinY = FMath::Clamp(FMath::FloorToInt(CyF - rCellV), 0, Res - 1);
	const int32 MaxY = FMath::Clamp(FMath::CeilToInt (CyF + rCellV), 0, Res - 1);

	bool bChanged = false;
	for (int32 Y = MinY; Y <= MaxY; ++Y)
	{
		const float ndy = (static_cast<float>(Y) + 0.5f - CyF) / rCellV;
		for (int32 X = MinX; X <= MaxX; ++X)
		{
			const float ndx = (static_cast<float>(X) + 0.5f - CxF) / rCellU;
			const float Dist = FMath::Sqrt(ndx * ndx + ndy * ndy);
			if (Dist > 1.0f)
			{
				continue;
			}

			// 안쪽 70%는 완전 공개, 바깥 30%는 선형 페이드 → 안개 경계가 부드럽다.
			const float Falloff = Dist <= 0.7f ? 1.0f : 1.0f - (Dist - 0.7f) / 0.3f;
			const uint8 NewVal = static_cast<uint8>(FMath::Clamp(Falloff * 255.0f, 0.0f, 255.0f));

			const int32 Idx = Y * Res + X;
			if (NewVal > RevealMask[Idx])
			{
				RevealMask[Idx] = NewVal;
				bChanged = true;
			}
		}
	}

	if (bChanged)
	{
		bRevealMaskDirty = true;
	}
	return bChanged;
}

void URetrieveMapSubsystem::FlushRevealMaskToTexture()
{
	if (!bRevealMaskDirty || !RevealMaskTexture)
	{
		return;
	}

	const int32 Res = RevealResolution;
	if (RevealMask.Num() != Res * Res)
	{
		return;
	}

	const int32 NumPixels = Res * Res;
	uint8* Buffer = new uint8[NumPixels * 4];
	for (int32 i = 0; i < NumPixels; ++i)
	{
		const uint8 Explored = RevealMask[i];
		Buffer[i * 4 + 0] = 255;                              // B
		Buffer[i * 4 + 1] = 255;                              // G
		Buffer[i * 4 + 2] = 255;                              // R
		Buffer[i * 4 + 3] = static_cast<uint8>(255 - Explored); // A = 안개 불투명도
	}

	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, Res, Res);
	RevealMaskTexture->UpdateTextureRegions(
		0, 1, Region,
		static_cast<uint32>(Res * 4), 4,
		Buffer,
		[](uint8* Src, const FUpdateTextureRegion2D* Reg)
		{
			delete[] Src;
			delete Reg;
		});

	bRevealMaskDirty = false;
}

bool URetrieveMapSubsystem::IsWorldLocationExplored(const FVector& WorldLocation) const
{
	const int32 Res = RevealResolution;
	if (RevealMask.Num() != Res * Res)
	{
		return false;
	}

	const FVector2D UV = WorldToUV(WorldLocation);
	const int32 X = FMath::Clamp(FMath::FloorToInt(UV.X * Res), 0, Res - 1);
	const int32 Y = FMath::Clamp(FMath::FloorToInt(UV.Y * Res), 0, Res - 1);
	return RevealMask[Y * Res + X] >= static_cast<uint8>(FMath::Clamp(ExploredThreshold, 0, 255));
}

UTexture2D* URetrieveMapSubsystem::GetRevealMaskTexture()
{
	EnsureRevealMask();
	FlushRevealMaskToTexture();
	return RevealMaskTexture;
}

void URetrieveMapSubsystem::SetRevealMaskData(const TArray<uint8>& InData, int32 InResolution)
{
	if (InResolution <= 0 || InData.Num() != InResolution * InResolution)
	{
		return;
	}

	RevealResolution = InResolution;
	RevealMask = InData;
	RevealMaskTexture = nullptr; // 해상도 변동 대비 재생성
	bRevealMaskDirty = true;
	EnsureRevealMask();
	FlushRevealMaskToTexture();
}
