#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Data/RetrieveMapIconRegistry.h"
#include "Data/RetrieveMapIconDataAsset.h"
#include "Subsystems/RetrieveMapSubsystem.h"
#include "UI/RetrieveGamePanelWidget.h"
#include "RetrieveWorldMapWidget.generated.h"

class URetrieveMapSubsystem;
class URetrieveMapIconComponent;
class ARetrieveBonfireActor;
class APlayerController;
class UTexture2D;
class UUserWidget;
class UWidget;
class URetrieveMapConfigDataAsset;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * 월드맵에서 활성화된 화톳불 아이콘을 더블클릭했을 때 발생.
 * BonfireId: 화톳불 고유 ID, BonfireDisplayName: 다이얼로그 표시용 이름
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnBonfireIconDoubleClicked, FName, BonfireId, FText, BonfireDisplayName);

/**
 * 전체화면 월드맵 패널 (독립 창으로 토글)
 *
 * WBP 설정:
 *   1. 미니맵과 동일한 BakedMapTexture 할당
 *   2. IconRegistry에 URetrieveMapIconRegistry 오브젝트 할당
 *   3. PlayerMarkerTexture에 방향 아이콘 할당
 *   4. PlayerController에서 ToggleKey 주입 → 동일 키로 열기/닫기
 *      (열 때 CenterOnPlayer() 호출 권장)
 *
 * 조작:
 *   - 마우스 휠     : 커서 기준 줌 인/아웃
 *   - 좌클릭 드래그 : 패닝
 *   - ToggleKey     : 닫기 (RetrieveGamePanelWidget이 처리)
 *   - BP에서 CenterOnPlayer() : 플레이어 위치로 뷰 이동
 */
UCLASS()
class RETRIEVE_API URetrieveWorldMapWidget : public URetrieveGamePanelWidget
{
	GENERATED_BODY()

public:
	URetrieveWorldMapWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual FGameplayTag GetPanelOpenSoundContext() const override;
	virtual FGameplayTag GetPanelCloseSoundContext() const override;

public:

	// 탑뷰 베이크 텍스처 (UV 규약: U=동쪽, V=0=북쪽)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap")
	TObjectPtr<UTexture2D> BakedMapTexture;

	/**
	 * 맵 텍스처를 그릴 때 사용할 UI 머티리얼 (선택).
	 * 캡처 알파가 반전(지형 A=0, 빈공간 A=255)돼 있으므로 머티리얼에서 Opacity=1-Tex.A 로
	 * 보정하면 빈 공간이 투명해진다. 'MapTex' 텍스처 파라미터에 맵 텍스처가 주입된다.
	 * 미설정 시 기존처럼 텍스처를 그대로(불투명) 그린다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap")
	TObjectPtr<UMaterialInterface> MapDisplayMaterial;

	// 플레이어 방향 마커 텍스처 (위쪽이 북쪽)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap")
	TObjectPtr<UTexture2D> PlayerMarkerTexture;

	// 미니맵과 동일한 레지스트리 오브젝트
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap")
	TObjectPtr<URetrieveMapIconRegistry> IconRegistry;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap")
	float PlayerMarkerSize = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap")
	FLinearColor PlayerMarkerColor = FLinearColor::White;

	// Independent panel chrome can take space around the actual map viewport.
	/**
	 * MapViewport 기준 오프셋 (WBP Border_Window 내부 기준 left/top/right/bottom).
	 * WBP에서 MapViewport 슬롯을 바꾸면 이 값도 맞춰 업데이트할 것.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap|Layout")
	FMargin MapViewportPadding = FMargin(98.0f, 74.0f, 117.0f, 72.0f);

	/**
	 * WBP에서 Border_Window가 차지하는 화면 비율 (Min = 좌상단, Max = 우하단).
	 * 기본값은 WBP의 Border_Window 앵커 (0.02, 0.04) ~ (0.98, 0.96).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap|Layout")
	FVector2D MapWindowAnchorMin = FVector2D(0.02f, 0.04f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap|Layout")
	FVector2D MapWindowAnchorMax = FVector2D(0.98f, 0.96f);

	// true(기본): 종횡비 무시, 뷰포트를 가득 채움
	// false: 텍스처/레벨 종횡비를 유지하며 뷰포트 안에서 최대 크기로 그림 (letterbox)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap|Layout")
	bool bStretchMapToViewport = true;

	/**
	 * 기준(fit) 크기에 곱하는 추가 배율 (가로/세로 독립). 1 = 기준 그대로.
	 * 값을 키우면 텍스처가 영역을 더 채우고(넘치면 클리핑), 가로/세로를 따로 줘서 늘릴 수 있다.
	 * 텍스처와 아이콘에 동일하게 적용되므로 아이콘은 항상 텍스처와 정합을 유지한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap|Layout")
	FVector2D MapTextureScale = FVector2D(1.0f, 1.0f);

	/**
	 * 맵 텍스처를 뷰포트 영역 내에서 미세 이동(픽셀, zoom=1 기준). 텍스처와 아이콘에 동일 적용.
	 * 텍스처 콘텐츠가 한쪽으로 치우쳐 보일 때 중심을 맞추는 용도.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap|Layout")
	FVector2D MapTextureOffset = FVector2D(0.0f, 0.0f);

	// 1 = 맵 전체 표시, 값이 클수록 확대
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap", meta=(ClampMin="1.0"))
	float MinZoom = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap", meta=(ClampMin="1.0"))
	float MaxZoom = 8.0f;

	// 마우스 휠 1틱당 줌 변화량
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap", meta=(ClampMin="0.1"))
	float ZoomStep = 0.5f;

	// ── 빠른 이동 델리게이트 ─────────────────────────────────────────────────
	/** 활성화된 화톳불 아이콘 더블클릭 시 발생. BP에서 WBP_FastTravelDialog 오픈에 사용 */
	UPROPERTY(BlueprintAssignable, Category="Retrieve|WorldMap|FastTravel")
	FOnBonfireIconDoubleClicked OnBonfireIconDoubleClicked;

	/** 화톳불 아이콘 히트 판정 반경 (픽셀). 크게 잡을수록 클릭하기 쉬움 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap|FastTravel", meta=(ClampMin="8.0"))
	float BonfireIconHitRadius = 48.0f;

	/** Slate 더블클릭 이벤트가 전달되지 않는 경우에도 사용할 연속 클릭 허용 시간 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap|FastTravel", meta=(ClampMin="0.1", ClampMax="1.0"))
	float BonfireDoubleClickInterval = 0.35f;

	/** true면 C++에서 빠른 이동 확인 창을 직접 생성한다. BP 델리게이트 연결 누락의 영향을 받지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap|FastTravel")
	bool bOpenFastTravelDialogInCpp = true;

	// ── 정적 월드맵 아이콘 DataAsset ─────────────────────────────────────────
	/**
	 * 에디터에서 "Refresh From Level" 버튼으로 자동 생성된 아이콘 DataAsset.
	 * WP 로드 여부와 무관하게 항상 표시된다.
	 *
	 * 설정 방법:
	 *   1. 콘텐츠 브라우저 → 우클릭 → Miscellaneous → Data Asset → RetrieveMapIconDataAsset 생성
	 *   2. 에셋 열기 → "Refresh From Level" 버튼 클릭 → 저장
	 *   3. 이 슬롯에 해당 에셋 할당
	 *   4. 아이콘 추가/이동 시 2번 반복
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap")
	TObjectPtr<URetrieveMapIconDataAsset> WorldMapIconData;

	// ── 모닥불 아이콘 색상 ─────────────────────────────────────────────────
	/** 활성화된 모닥불 아이콘 색상 (빠른 이동 가능) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap|Bonfire")
	FLinearColor BonfireActivatedColor = FLinearColor(1.0f, 0.75f, 0.2f, 1.0f);

	/** 비활성화 모닥불 아이콘 색상 (빠른 이동 불가) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap|Bonfire")
	FLinearColor BonfireInactiveColor = FLinearColor(0.5f, 0.5f, 0.5f, 0.8f);

	/** 활성화 여부 판정 시 사용하는 위치 매칭 허용 오차 (언리얼 단위) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap|Bonfire", meta=(ClampMin="50.0"))
	float BonfireActivationCheckRadius = 300.0f;

	// ── 아이콘 필터 ──────────────────────────────────────────────────────────
	/**
	 * 월드맵에서 숨길 아이콘 타입 목록.
	 * 여기에 추가된 타입은 월드맵에 표시되지 않음.
	 * 비어있으면 등록된 모든 아이콘이 표시됨.
	 * 미니맵의 bShowOnMinimap 설정과 독립적으로 동작.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap|IconFilter")
	TSet<ERetrieveMapIconType> HiddenIconTypesOnWorldMap;

	/** 특정 아이콘 타입의 월드맵 표시 여부를 설정 */
	UFUNCTION(BlueprintCallable, Category="Retrieve|WorldMap|IconFilter")
	void SetIconTypeVisible(ERetrieveMapIconType IconType, bool bVisible);

	/** 특정 아이콘 타입이 월드맵에 표시되는지 반환 */
	UFUNCTION(BlueprintPure, Category="Retrieve|WorldMap|IconFilter")
	bool IsIconTypeVisible(ERetrieveMapIconType IconType) const;

	/** 빠른 이동 확인 창. WBP_FastTravelDialog를 직접 할당 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap|FastTravel")
	TSubclassOf<UUserWidget> FastTravelDialogClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap|FastTravel")
	TSubclassOf<UUserWidget> FastTravelLoadingClass;

	// 플레이어 위치로 뷰 이동 (월드맵 열 때 호출 권장)
	UFUNCTION(BlueprintCallable, Category="Retrieve|WorldMap")
	void CenterOnPlayer();

	UFUNCTION(BlueprintCallable, Category="Retrieve|WorldMap")
	void ZoomIn();

	UFUNCTION(BlueprintCallable, Category="Retrieve|WorldMap")
	void ZoomOut();

	// 현재 줌 레벨 읽기
	UFUNCTION(BlueprintPure, Category="Retrieve|WorldMap")
	float GetZoomLevel() const { return ZoomLevel; }

	// ── 사용자 웨이포인트 마커 ──────────────────────────────────────────────
	/** T 키로 찍은 웨이포인트 마커 텍스처 (null이면 색상 박스로 대체) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap|Waypoint")
	TObjectPtr<UTexture2D> WaypointMarkerTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap|Waypoint", meta=(ClampMin="8.0"))
	float WaypointMarkerSize = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap|Waypoint")
	FLinearColor WaypointMarkerColor = FLinearColor(1.0f, 0.25f, 0.25f, 1.0f);

	// ── 레이블 텍스트 설정 ──────────────────────────────────────────────────
	// 플레이어 마커 위에 표시할 텍스트
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap|Label")
	FText CurrentLocationText = FText::FromString(TEXT("현재 위치"));

	// 플레이어 레이블 폰트 크기
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap|Label", meta=(ClampMin="8"))
	int32 PlayerLabelFontSize = 14;

	// POI 아이콘 레이블 폰트 크기
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap|Label", meta=(ClampMin="8"))
	int32 IconLabelFontSize = 10;

	// 텍스트 색상
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap|Label")
	FLinearColor LabelColor = FLinearColor::White;

	// 텍스트 드롭섀도우 색상
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap|Label")
	FLinearColor LabelShadowColor = FLinearColor(0.f, 0.f, 0.f, 0.8f);

	// 자동 맵 설정 DataAsset
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Minimap")
	TObjectPtr<URetrieveMapConfigDataAsset> MapConfig;

protected:
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> MapViewport;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> HorizontalBox_Body;

	// 마우스 휠·클릭 입력 수신을 위한 필수 오버라이드
	virtual bool NativeIsInteractable() const override { return true; }
	virtual bool NativeSupportsKeyboardFocus() const override { return true; }

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled
	) const override;

	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/** T 키: 커서 위치 → 웨이포인트 추가 / Delete: 전체 초기화 */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	float     ZoomLevel    = 1.0f;
	FVector2D ViewCenterUV = FVector2D(0.5f, 0.5f);

	bool      bIsPanning       = false;
	bool      bPendingCenterOnPlayer = true;
	FVector2D PanStartLocalPos;
	FVector2D PanStartCenterUV;

	// NativePaint에서 실제 사용된 드로잉 파라미터 캐시.
	// HitTestBonfireIcon이 동일한 좌표계로 판정하도록 사용.
	mutable FVector2D CachedPaintCenter   = FVector2D(0.5f, 0.5f);
	mutable float     CachedPaintScaledW  = 1.0f;
	mutable float     CachedPaintScaledH  = 1.0f;

	// 디버그: 맵 뷰 사각형 계산 경로/결과를 값이 바뀔 때만 1회 로깅하기 위한 캐시.
	mutable FString   DbgLastRectPath;
	mutable FVector2D DbgLastRectTopLeft = FVector2D(-1.f, -1.f);
	mutable FVector2D DbgLastRectSize    = FVector2D(-1.f, -1.f);

	// MapDisplayMaterial로부터 생성한 동적 머티리얼 인스턴스(맵 텍스처 파라미터 주입용) 캐시.
	UPROPERTY(Transient)
	mutable TObjectPtr<UMaterialInstanceDynamic> MapDisplayMID;

	// ViewCenterUV를 맵 범위 내로 클램핑
	void ClampViewCenter();

	/**
	 * 로컬 좌표 HitPos 근처에 있는 활성화된 화톳불 아이콘을 탐색.
	 * BonfireIconHitRadius 이내에 있으면 해당 MapIconComponent를 반환, 없으면 nullptr.
	 */
	const URetrieveMapIconComponent* HitTestBonfireIcon(
		const FGeometry& InGeometry,
		const FVector2D& HitPos) const;

	/** 활성화된 화톳불 아이콘이면 BP 델리게이트를 브로드캐스트한다. */
	bool TryBroadcastBonfireDoubleClick(
		const FGeometry& InGeometry,
		const FVector2D& HitPos);

	bool HandleBonfireIconClick(
		const FGeometry& InGeometry,
		const FVector2D& HitPos);

	bool TryOpenFastTravelDialog(const ARetrieveBonfireActor& Bonfire);

	bool IsBonfireEntryActivated(const FRetrieveMapIconEntry& Entry) const;

	/**
	 * 에디터에서 활성으로 배치된 모닥불(bStartActivated)을 세이브 서브시스템에
	 * 인메모리로 시드한다. WP 스트리밍/액터 로드 여부와 무관하게 빠른이동 Transform을 확보.
	 */
	void SeedDefaultActivatedBonfires();

	UFUNCTION()
	void HandleFastTravelConfirmClicked();

	UFUNCTION()
	void HandleFastTravelCancelClicked();

	UFUNCTION()
	void HandleFastTravelCompleted();

	void CloseActiveFastTravelDialog();
	void ShowFastTravelLoadingOverlay(APlayerController* PC);

	/** 더블클릭이 소비됐을 때 MouseButtonDown의 패닝 시작을 막기 위한 플래그 */
	bool bDoubleClickConsumed = false;

	/** 마우스 캡처 때문에 Slate 더블클릭 이벤트가 누락되는 경우를 위한 보조 판정 상태 */
	double LastBonfireIconClickTime = -1.0;
	FName LastClickedBonfireId = NAME_None;
	FVector2D LastBonfireIconClickPos = FVector2D::ZeroVector;
	bool bWasLeftMouseButtonDown = false;

	UPROPERTY()
	TObjectPtr<UUserWidget> ActiveFastTravelDialog;

	UPROPERTY()
	TObjectPtr<UUserWidget> ActiveFastTravelLoadingOverlay;

	FName ActiveFastTravelBonfireId = NAME_None;

	/**
	 * 레벨 종횡비를 반영한 맵 표시 크기 계산.
	 * ZoomLevel 적용 전 기준 크기(BaseW, BaseH)를 반환.
	 * MapSide(짧은 변) 안에 레벨 비율대로 최대한 채움.
	 */
	void ComputeBaseMapSize(const URetrieveMapSubsystem* MapSub,
	                        const FVector2D& MapViewSize,
	                        float& OutBaseW, float& OutBaseH) const;

	/**
	 * TextureUV → 위젯 로컬 화면 좌표.
	 * ScaledW/ScaledH = BaseW/BaseH * ZoomLevel (축별 독립 스케일).
	 */
	FVector2D UVToScreen(const FVector2D& UV, const FVector2D& Center,
	                     float ScaledW, float ScaledH) const;

	/** UVToScreen 역함수: 위젯 로컬 좌표 → TextureUV */
	FVector2D ScreenToUV(const FVector2D& ScreenPos, const FVector2D& Center,
	                     float ScaledW, float ScaledH) const;

	void GetMapViewRect(const FGeometry& AllottedGeometry, FVector2D& OutTopLeft, FVector2D& OutSize) const;

	/** 맵 텍스처/아이콘 공통 중심 = 뷰포트 중앙 + MapTextureOffset. 텍스처와 아이콘 정합 보장. */
	FVector2D GetMapDrawCenter(const FVector2D& MapViewTopLeft, const FVector2D& MapViewSize) const;

	// 디버그: 맵 뷰 사각형 계산 경로와 결과를 값이 바뀔 때만 1회 로깅(매 프레임 스팸 방지).
	void LogMapViewRect(const TCHAR* PathName, const FVector2D& WidgetSize,
	                    const FVector2D& TopLeft, const FVector2D& Size) const;
	bool IsInsideMapView(const FVector2D& LocalPosition, const FVector2D& MapTopLeft, const FVector2D& MapSize) const;
	APlayerController* GetWorldMapPlayerController() const;

	void DrawWorldIcon(
		FSlateWindowElementList& OutDrawElements,
		int32& LayerId,
		const FGeometry& AllottedGeometry,
		const FRetrieveMapIconSnapshot& Snap,
		const FVector2D& ScreenPos
	) const;

	// 드롭섀도우 포함 텍스트 드로우 (CenterX 기준 수평 중앙 정렬)
	void DrawLabel(
		FSlateWindowElementList& OutDrawElements,
		int32& LayerId,
		const FGeometry& AllottedGeometry,
		const FString& Text,
		const FVector2D& CenterPos,
		const FSlateFontInfo& FontInfo,
		const FLinearColor& Color
	) const;
};
