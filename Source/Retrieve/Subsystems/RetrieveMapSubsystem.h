#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"
#include "Data/RetrieveMapIconRegistry.h"
#include "RetrieveMapSubsystem.generated.h"

class URetrieveMapIconComponent;
class UTexture2D;
class URetrieveMapConfigDataAsset;

/**
 * 월드맵 전용 아이콘 스냅샷.
 * 액터가 World Partition으로 언로드되어도 위치/타입 정보가 유지된다.
 */
USTRUCT(BlueprintType)
struct FRetrieveMapIconSnapshot
{
	GENERATED_BODY()

	UPROPERTY() FVector WorldLocation = FVector::ZeroVector;
	UPROPERTY() ERetrieveMapIconType IconType = ERetrieveMapIconType::POI;
	UPROPERTY() FText MapLabel;
	UPROPERTY() bool bShowLabelOnWorldMap = true;
	UPROPERTY() bool bOverrideIcon = false;
	UPROPERTY() TObjectPtr<UTexture2D> OverrideTexture;
	UPROPERTY() FLinearColor OverrideColor = FLinearColor::White;
	UPROPERTY() float OverrideSize = 16.0f;
};

/**
 * 월드맵 타일 하나.
 * 맵 전체를 NxN 그리드로 분할할 때 각 타일의 텍스처와 UV 범위를 저장.
 *
 * 예) 2×2 그리드:
 *   좌상(NW): UVMin=(0,0)     UVMax=(0.5,0.5)
 *   우상(NE): UVMin=(0.5,0)   UVMax=(1.0,0.5)
 *   좌하(SW): UVMin=(0,0.5)   UVMax=(0.5,1.0)
 *   우하(SE): UVMin=(0.5,0.5) UVMax=(1.0,1.0)
 */
USTRUCT(BlueprintType)
struct FRetrieveMapTile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Minimap|Tiles")
	TObjectPtr<UTexture2D> Texture;

	/** 이 타일이 커버하는 전체 맵 UV 범위 — Min */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Minimap|Tiles")
	FVector2D UVMin = FVector2D(0.0f, 0.0f);

	/** 이 타일이 커버하는 전체 맵 UV 범위 — Max */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Minimap|Tiles")
	FVector2D UVMax = FVector2D(1.0f, 1.0f);

	bool HasTexture() const { return Texture != nullptr; }

	/** 지정 UV 범위와 겹치는지 (뷰포트 컬링용) */
	bool Overlaps(const FVector2D& ViewMin, const FVector2D& ViewMax) const
	{
		return UVMin.X < ViewMax.X && UVMax.X > ViewMin.X &&
		       UVMin.Y < ViewMax.Y && UVMax.Y > ViewMin.Y;
	}
};

/**
 * 사용자가 월드맵에서 T 키로 찍은 웨이포인트 하나.
 * WaypointId: 서브시스템이 자동 부여하는 고유 번호 (제거 키로 사용).
 */
USTRUCT(BlueprintType)
struct FUserWaypoint
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Retrieve|Waypoint")
	int32 WaypointId = -1;

	UPROPERTY(BlueprintReadWrite, Category="Retrieve|Waypoint")
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category="Retrieve|Waypoint")
	FText Label;

	UPROPERTY(BlueprintReadWrite, Category="Retrieve|Waypoint")
	FLinearColor Color = FLinearColor(1.0f, 0.25f, 0.25f, 1.0f);
};

/**
 * 미니맵 시스템의 중앙 관리자.
 *
 * UV 축 규약 (north-up):
 *   U = (WorldY - MapOrigin.Y) / MapExtent  → 동쪽(+Y)이 U 증가 방향
 *   V = 1 - (WorldX - MapOrigin.X) / MapExtent  → 북쪽(+X)이 V=0 (텍스처 위쪽)
 *
 * 이 규약은 SceneCapture Pitch=-90, Yaw=0으로 캡처한 탑뷰 베이크 텍스처와 일치한다.
 */
UCLASS(BlueprintType)
class RETRIEVE_API URetrieveMapSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// Landscape BeginPlay 이후 시점에 초기화
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/**
	 * 자동 맵 설정 DataAsset.
	 *
	 * BP_MapCaptureActor / RefreshMapAll()에서 자동 갱신되는 값들을 저장한다.
	 *
	 * 포함 데이터:
	 *   - BakedMapTexture
	 *   - MapOrigin
	 *   - MapExtentXY
	 *   - WorldMapIconData
	 *   - IconRegistry
	 *
	 * 이 값이 설정되어 있으면 InitializeBounds()에서
	 * SceneCapture2D 검색이나 Landscape bounds 계산보다 먼저 사용한다.
	 *
	 * 목적:
	 *   - 월드맵 / 미니맵 / 나침반 좌표계 통일
	 *   - 맵 변경 시 수동 좌표 입력 제거
	 *   - World Partition 로딩 타이밍 영향 최소화
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Map")
	TObjectPtr<URetrieveMapConfigDataAsset> MapConfig;

	// 맵 원점 (월드 X min, Y min)
	UPROPERTY(BlueprintReadOnly, Category="Retrieve|Minimap")
	FVector2D MapOrigin = FVector2D::ZeroVector;

	// 정사각 맵 한 변 크기 (UU) — 미니맵 줌 계산용, max(Width, Height)
	UPROPERTY(BlueprintReadOnly, Category="Retrieve|Minimap")
	float MapExtent = 1.0f;

	/**
	 * 실제 레벨의 X/Y 월드 범위 (UU).
	 *   X = WorldX 범위 (North-South, V 방향)
	 *   Y = WorldY 범위 (East-West,   U 방향)
	 * 직사각형 레벨에서 WorldToUV의 축별 정규화에 사용.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Retrieve|Minimap")
	FVector2D MapExtentXY = FVector2D(1.0f, 1.0f);

	// 에디터에서 할당하거나 코드에서 SetBakedMapTexture로 등록
	// 타일(MapTiles)이 설정되면 이 텍스처는 미니맵 머티리얼용으로만 사용됨
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Minimap")
	TObjectPtr<UTexture2D> BakedMapTexture;

	/**
	 * 월드맵 타일 배열.
	 * 비어있으면 BakedMapTexture 단일 텍스처 모드.
	 * 값이 있으면 타일 모드 — 월드맵은 뷰포트에 겹치는 타일만 그려 메모리·품질 모두 개선.
	 *
	 * 에디터 설정 예 (2×2 그리드):
	 *   [0] UVMin=(0,0)     UVMax=(0.5,0.5)  → NW 타일 텍스처
	 *   [1] UVMin=(0.5,0)   UVMax=(1,0.5)    → NE 타일 텍스처
	 *   [2] UVMin=(0,0.5)   UVMax=(0.5,1)    → SW 타일 텍스처
	 *   [3] UVMin=(0.5,0.5) UVMax=(1,1)      → SE 타일 텍스처
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Minimap|Tiles")
	TArray<FRetrieveMapTile> MapTiles;

	/** 타일 모드인지 여부 */
	bool HasTiles() const { return MapTiles.Num() > 0; }

	/**
	 * UV 좌표를 포함하는 타일 텍스처 반환 (미니맵 머티리얼용).
	 * 타일 미설정 시 BakedMapTexture 반환.
	 */
	UTexture2D* GetTextureForUV(const FVector2D& UV) const;

	UFUNCTION(BlueprintCallable, Category="Retrieve|Minimap")
	void SetBakedMapTexture(UTexture2D* InTexture);

	// 경계 수동 설정 — 정사각 레벨 (Landscape 없는 레벨용)
	UFUNCTION(BlueprintCallable, Category="Retrieve|Minimap")
	void SetMapBounds(FVector2D InOrigin, float InExtent);

	/**
	 * 경계 수동 설정 — 직사각 레벨 / SceneCapture 범위 직접 지정용.
	 * InExtentX : WorldX 범위 (North-South, V 방향)
	 * InExtentY : WorldY 범위 (East-West,   U 방향)
	 */
	UFUNCTION(BlueprintCallable, Category="Retrieve|Minimap")
	void SetMapBoundsXY(FVector2D InOrigin, float InExtentX, float InExtentY);

	/**
	 * DA_MapConfig를 (없으면) 로드하고 그 bounds/텍스처를 즉시 적용한다.
	 * MapConfig 값은 SceneCapture/Landscape 폴백보다 항상 우선한다.
	 * 매 틱 호출해도 저렴(이미 로드+유효하면 즉시 반환). 미니맵이 게임 시작 시
	 * 월드맵을 열지 않고도 올바른 좌표/텍스처를 쓰도록 보장한다.
	 */
	UFUNCTION(BlueprintCallable, Category="Retrieve|Minimap")
	void EnsureMapConfigLoaded();

	UFUNCTION(BlueprintCallable, Category="Retrieve|Map")
	bool InitializeMapConfigRuntime(URetrieveMapConfigDataAsset* OverrideConfig = nullptr);
	
	/**
	 * WorldLocation → 텍스처 UV [0,1].
	 *   U = (WorldY - OriginY) / MapExtentXY.Y   (East-West)
	 *   V = 1 - (WorldX - OriginX) / MapExtentXY.X  (North-South, 위쪽=North)
	 */
	FVector2D WorldToUV(const FVector& WorldLocation) const;

	/**
	 * 텍스처 UV [0,1] → 월드 XY 좌표 (WorldToUV 역함수).
	 * Z = 0 으로 반환; 필요하면 호출자가 Z를 설정할 것.
	 */
	FVector UVToWorld(const FVector2D& UV) const;

	// ── 사용자 웨이포인트 ────────────────────────────────────────────────────

	/**
	 * 웨이포인트를 추가하고 부여된 WaypointId를 반환.
	 * Label이 비어있으면 자동으로 "[N]" 형식으로 설정.
	 */
	UFUNCTION(BlueprintCallable, Category="Retrieve|Waypoint")
	int32 AddUserWaypoint(
		FVector InWorldLocation,
		FText InLabel = FText::GetEmpty(),
		FLinearColor InColor = FLinearColor(1.0f, 0.25f, 0.25f, 1.0f)
	);

	/** WaypointId로 웨이포인트 제거. 없으면 false 반환 */
	UFUNCTION(BlueprintCallable, Category="Retrieve|Waypoint")
	bool RemoveUserWaypointById(int32 InWaypointId);

	/** 모든 웨이포인트 삭제 */
	UFUNCTION(BlueprintCallable, Category="Retrieve|Waypoint")
	void ClearUserWaypoints();

	/** 전체 웨이포인트 배열 읽기 (렌더링/나침반용) */
	UFUNCTION(BlueprintPure, Category="Retrieve|Waypoint")
	const TArray<FUserWaypoint>& GetUserWaypoints() const { return UserWaypoints; }

	/**
	 * GetZoom: 미니맵 머티리얼 Zoom 파라미터 계산.
	 * min(ExtentX, ExtentY) 기준 → 짧은 축이 정확히 ViewWorldRadius를 채움.
	 */
	float GetZoom(float ViewWorldRadius) const;

	/** 현재 감지된 바운드를 Output Log에 출력 (BP에서 호출 가능) */
	UFUNCTION(BlueprintCallable, Category="Retrieve|Minimap")
	void DebugPrintBounds() const;

	void RegisterIcon(URetrieveMapIconComponent* Icon);
	void UnregisterIcon(URetrieveMapIconComponent* Icon);

	/**
	 * 현재 월드에 로드된 모든 MapIconComponent를 스캔해서 스냅샷을 갱신.
	 * Is Spatially Loaded=false 액터의 타이밍 등록 누락을 보완하기 위해
	 * 월드맵 패널을 열 때 호출한다.
	 */
	UFUNCTION(BlueprintCallable, Category="Retrieve|Minimap")
	void RefreshWorldMapSnapshots();

	/** 미니맵용: 현재 로드된 아이콘 컴포넌트 목록 */
	const TArray<TObjectPtr<URetrieveMapIconComponent>>& GetIcons() const { return Icons; }

	/** 월드맵용: 한 번이라도 등록된 모든 아이콘의 스냅샷 (WP 언로드 후에도 유지) */
	const TArray<FRetrieveMapIconSnapshot>& GetIconSnapshots() const { return IconSnapshots; }

	/** 디버그: 현재 스냅샷 목록을 Output Log와 화면에 출력 */
	UFUNCTION(BlueprintCallable, Category="Retrieve|Minimap")
	void DebugPrintIconSnapshots() const;

	bool HasValidBounds() const { return MapExtent > 1.0f; }

private:
	bool InitializeBounds(UWorld& InWorld, bool bLogWarnings = true);
	void RetryInitializeBounds();

	UPROPERTY()
	TArray<TObjectPtr<URetrieveMapIconComponent>> Icons;

	/** 월드맵 전용 영구 스냅샷 배열 */
	UPROPERTY()
	TArray<FRetrieveMapIconSnapshot> IconSnapshots;

	/** 액터 → 스냅샷 인덱스 (중복 등록 방지 및 위치 갱신용) */
	TMap<FObjectKey, int32> SnapshotIndexByActor;

	FTimerHandle BoundsRetryTimerHandle;
	int32 BoundsRetryAttempts = 0;

	UPROPERTY()
	TArray<FUserWaypoint> UserWaypoints;

	int32 NextWaypointId = 0;
};