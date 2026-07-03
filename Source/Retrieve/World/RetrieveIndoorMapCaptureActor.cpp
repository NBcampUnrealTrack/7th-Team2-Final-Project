#include "World/RetrieveIndoorMapCaptureActor.h"

#include "Components/BoxComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Data/RetrieveMinimapAreaDataAsset.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "World/RetrieveMinimapAreaVolume.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

ARetrieveIndoorMapCaptureActor::ARetrieveIndoorMapCaptureActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	SceneCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneCapture"));
	SetRootComponent(SceneCapture);
	SceneCapture->ProjectionType = ECameraProjectionMode::Orthographic;
	SceneCapture->bCaptureEveryFrame = false;
	SceneCapture->bCaptureOnMovement = false;
	SceneCapture->CaptureSource = ESceneCaptureSource::SCS_BaseColor;
	SceneCapture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;

	IndoorNamePatterns = { TEXT("Interior"), TEXT("Dungeon"), TEXT("Cave") };
}

void ARetrieveIndoorMapCaptureActor::RequestCapture(ARetrieveMinimapAreaVolume* Area)
{
	if (!IsValid(Area) || !SharedRenderTarget || !GetWorld())
	{
		return;
	}

	if ((LastCapturedArea == Area && Area->GetRuntimeTexture() == SharedRenderTarget) ||
		PendingArea == Area)
	{
		return;
	}

	PendingArea = Area;
	GetWorldTimerManager().SetTimer(
		CaptureTimerHandle,
		this,
		&ARetrieveIndoorMapCaptureActor::CapturePendingArea,
		CaptureDelay,
		false);
}

void ARetrieveIndoorMapCaptureActor::ConfigureCapture()
{
	if (!SceneCapture)
	{
		return;
	}

	SceneCapture->CaptureSource = ESceneCaptureSource::SCS_BaseColor;

	FEngineShowFlags& Flags = SceneCapture->ShowFlags;
	Flags.SetAtmosphere(false);
	Flags.SetFog(false);
	Flags.SetVolumetricFog(false);
	Flags.SetCloud(false);
	Flags.SetBloom(false);
	Flags.SetEyeAdaptation(false);
	Flags.SetMotionBlur(false);
	Flags.SetDepthOfField(false);
	Flags.SetLensFlares(false);
	Flags.SetVignette(false);
	Flags.SetTemporalAA(false);

	FPostProcessSettings& PostProcess = SceneCapture->PostProcessSettings;
	PostProcess.bOverride_AutoExposureMinBrightness = true;
	PostProcess.AutoExposureMinBrightness = 1.0f;
	PostProcess.bOverride_AutoExposureMaxBrightness = true;
	PostProcess.AutoExposureMaxBrightness = 1.0f;
	PostProcess.bOverride_BloomIntensity = true;
	PostProcess.BloomIntensity = 0.0f;
	SceneCapture->PostProcessBlendWeight = 1.0f;
}

void ARetrieveIndoorMapCaptureActor::CapturePendingArea()
{
	ARetrieveMinimapAreaVolume* Area = PendingArea.Get();
	PendingArea = nullptr;

	if (!IsValid(Area) || !SceneCapture || !SharedRenderTarget)
	{
		return;
	}

	const FBox AreaBox = Area->GetAreaBox();
	if (!AreaBox.IsValid)
	{
		return;
	}

	const FVector Center = AreaBox.GetCenter();
	const FVector Extent = AreaBox.GetExtent();
	const float SquareDiameter = FMath::Max(Extent.X, Extent.Y) * 2.0f;

	SetActorLocation(FVector(Center.X, Center.Y, AreaBox.Max.Z + CaptureHeight));
	SetActorRotation(FRotator(-90.0f, 0.0f, 0.0f));
	SceneCapture->TextureTarget = SharedRenderTarget;
	SceneCapture->OrthoWidth = FMath::Max(SquareDiameter, 1.0f);
	SceneCapture->ShowOnlyActors.Reset();
	SceneCapture->ClearShowOnlyComponents();

	int32 CapturedComponentCount = 0;
	if (IsValid(Area->SourceActor) && Area->bFilterSourceStaticMeshComponents)
	{
		TInlineComponentArray<UStaticMeshComponent*> StaticMeshComponents;
		Area->SourceActor->GetComponents(StaticMeshComponents);
		for (UStaticMeshComponent* Component : StaticMeshComponents)
		{
			if (!Area->ShouldCaptureStaticMeshComponent(Component) ||
				!Component->Bounds.GetBox().Intersect(AreaBox))
			{
				continue;
			}

			SceneCapture->ShowOnlyComponent(Component);
			++CapturedComponentCount;
		}
	}

	// Generic fallback for manually-authored areas without a component-filtered source.
	if (CapturedComponentCount == 0)
	{
		UWorld* World = GetWorld();
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Candidate = *It;
			if (!IsValid(Candidate) || Candidate == this || Candidate == Area ||
				Candidate->ActorHasTag(Area->CaptureExcludeTag))
			{
				continue;
			}

			const FBox CandidateBounds = Candidate->GetComponentsBoundingBox(true);
			if (CandidateBounds.IsValid && CandidateBounds.Intersect(AreaBox))
			{
				SceneCapture->ShowOnlyActors.Add(Candidate);
			}
		}
	}

	ConfigureCapture();
	SceneCapture->CaptureScene();
	if (IsValid(LastCapturedArea) && LastCapturedArea != Area)
	{
		LastCapturedArea->SetRuntimeTexture(nullptr);
	}
	Area->SetRuntimeTexture(SharedRenderTarget);
	LastCapturedArea = Area;

	UE_LOG(LogTemp, Log,
		TEXT("[Retrieve|IndoorMap] Captured area '%s' to '%s' (Components=%d Actors=%d OrthoWidth=%.0f)"),
		*GetNameSafe(Area),
		*GetNameSafe(SharedRenderTarget),
		CapturedComponentCount,
		SceneCapture->ShowOnlyActors.Num(),
		SceneCapture->OrthoWidth);
}

void ARetrieveIndoorMapCaptureActor::GenerateIndoorAreasFromNamedActors()
{
#if WITH_EDITOR
	UWorld* World = GetWorld();
	if (!World || !GEditor)
	{
		return;
	}

	TArray<AActor*> Candidates;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Candidate = *It;
		if (!IsValid(Candidate) || Candidate == this ||
			Candidate->IsA<ARetrieveMinimapAreaVolume>())
		{
			continue;
		}

		const FString SearchName = Candidate->GetActorLabel() + TEXT(" ") + Candidate->GetName();
		const bool bNameMatches = IndoorNamePatterns.ContainsByPredicate(
			[&SearchName](const FString& Pattern)
			{
				return !Pattern.IsEmpty() && SearchName.Contains(Pattern, ESearchCase::IgnoreCase);
			});

		if (Candidate->ActorHasTag(TEXT("Minimap.Indoor")) || bNameMatches)
		{
			Candidates.Add(Candidate);
		}
	}

	int32 CreatedCount = 0;
	for (AActor* Candidate : Candidates)
	{
		bool bAlreadyExists = false;
		for (TActorIterator<ARetrieveMinimapAreaVolume> AreaIt(World); AreaIt; ++AreaIt)
		{
			if (AreaIt->SourceActor == Candidate)
			{
				bAlreadyExists = true;
				break;
			}
		}
		if (bAlreadyExists)
		{
			continue;
		}

		const FBox Bounds = Candidate->GetComponentsBoundingBox(true);
		if (!Bounds.IsValid)
		{
			continue;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.OverrideLevel = Candidate->GetLevel();
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ARetrieveMinimapAreaVolume* Area = World->SpawnActor<ARetrieveMinimapAreaVolume>(
			ARetrieveMinimapAreaVolume::StaticClass(),
			Bounds.GetCenter(),
			FRotator::ZeroRotator,
			SpawnParameters);

		if (!Area)
		{
			continue;
		}

		Area->Modify();
		Area->SourceActor = Candidate;
		Area->AreaData = DefaultAutoAreaData;
		Area->AreaBounds->SetBoxExtent(Bounds.GetExtent() + FVector(100.0f, 100.0f, 50.0f));
		Area->SetActorLabel(FString::Printf(TEXT("MinimapArea_%s"), *Candidate->GetActorLabel()));
		Area->SetFolderPath(TEXT("Minimap/IndoorAreas"));
		Area->MarkPackageDirty();
		++CreatedCount;
	}

	UE_LOG(LogTemp, Log,
		TEXT("[Retrieve|IndoorMap] Auto setup finished: Candidates=%d Created=%d"),
		Candidates.Num(), CreatedCount);
#else
	UE_LOG(LogTemp, Warning,
		TEXT("[Retrieve|IndoorMap] Auto area generation is available only in editor builds."));
#endif
}
