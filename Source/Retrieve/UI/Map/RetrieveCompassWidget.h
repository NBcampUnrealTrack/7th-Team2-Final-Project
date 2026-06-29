#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectKey.h"
#include "Blueprint/UserWidget.h"
#include "Data/RetrieveMapIconDataAsset.h"
#include "Data/RetrieveMapIconRegistry.h"
#include "RetrieveCompassWidget.generated.h"

class UCanvasPanel;
class UTexture2D;
class URetrieveMapIconComponent;

/**
 * HUD 상시 표시 나침반 띠 위젯.
 *
 * WBP 설정:
 *   1. 이 클래스를 부모로 하는 WBP_Compass 생성
 *   2. 위젯 크기: 가로 800 × 세로 56 (픽셀, 조정 가능)
 *   3. HUD_FantasyWarrior_Compass_02 텍스처를 CompassBandTexture에 할당 (선택)
 *   4. WaypointMarkerTexture에 마커 아이콘 할당 (선택, 없으면 색상 박스)
 *   5. PlayerController / GamePanelWidget에서 AddToViewport(ZOrder=10) 호출
 *
 * 북쪽 규약: 월드 +X = 북(N), 카메라 Yaw 0 = 북쪽을 바라봄
 * 웨이포인트 방위: URetrieveMapSubsystem::GetUserWaypoints() 자동 참조
 */
UCLASS()
class RETRIEVE_API URetrieveCompassWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 나침반 띠에 표시될 시야각 (도). 클수록 더 넓은 범위의 방위가 보임.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Compass",
		meta=(ClampMin="30.0", ClampMax="360.0"))
	float FieldOfViewDeg = 120.0f;

	// 배경 텍스처 (null이면 반투명 검정 직사각형으로 대체)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Compass")
	TObjectPtr<UTexture2D> CompassBandTexture;

	// 웨이포인트 마커 텍스처 (null이면 단색 마름모)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Compass")
	TObjectPtr<UTexture2D> WaypointMarkerTexture;

	// 방위 눈금/레이블 색상
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Compass")
	FLinearColor TickColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.85f);

	// N/E/S/W 주요 방위 강조 색상
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Compass")
	FLinearColor CardinalColor = FLinearColor(1.0f, 0.85f, 0.2f, 1.0f);

	// 웨이포인트 마커 색상 (개별 색상 없을 때 폴백)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Compass")
	FLinearColor WaypointMarkerColor = FLinearColor(1.0f, 0.25f, 0.25f, 1.0f);

	// 웨이포인트 마커 크기 (픽셀)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Compass",
		meta=(ClampMin="4.0"))
	float WaypointMarkerSize = 12.0f;

	// 배경 색상 (CompassBandTexture 없을 때)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Compass")
	FLinearColor BackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.55f);

	// 주요 방위 레이블 폰트 크기
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Compass",
		meta=(ClampMin="8"))
	int32 CardinalFontSize = 14;

	// 보조 방위(NE/SE/SW/NW) 폰트 크기
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Compass",
		meta=(ClampMin="6"))
	int32 SubCardinalFontSize = 10;

	// 중앙 지시선 색상
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Compass")
	FLinearColor CenterTickColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

	// ── 외부 나침반 위젯 연동 옵션 ───────────────────────────────────────────
	/**
	 * false로 설정하면 NativePaint가 배경 박스를 그리지 않음.
	 * WBP에 FantasyWarrior 배경이 이미 있으면 반드시 false 로 설정.
	 * (true = 독립형 나침반 / false = WBP 자체 배경 사용)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Compass|Layout")
	bool bDrawBuiltinBackground = true;

	/**
	 * false로 설정하면 N/NE/E/SE/S/SW/W/NW 눈금·레이블을 그리지 않음.
	 * WBP에 Compass_Content 위젯이 이미 방위를 표시하면 반드시 false 로 설정.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Compass|Layout")
	bool bDrawBuiltinCardinals = true;

	/**
	 * 나침반 띠 가로선 Y 위치 비율 (0=위, 1=아래).
	 * 웨이포인트 마커와 중앙 지시선이 이 위치에 그려진다.
	 * WBP_Compass(900×150)에서 Compass_Content는 약 y=75px 근처가 띠 중앙.
	 * → 기본값 0.5 유지, 실제 맞지 않으면 WBP Class Defaults에서 조정.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Compass|Layout",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float CompassBandYRatio = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Compass|WorldIcons")
	TObjectPtr<URetrieveMapIconDataAsset> WorldMapIconData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Compass|WorldIcons")
	TObjectPtr<URetrieveMapIconRegistry> IconRegistry;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Compass|WorldIcons")
	TSet<ERetrieveMapIconType> HiddenIconTypesOnCompass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Compass|WorldIcons", meta=(ClampMin="4.0"))
	float CompassIconSizeScale = 0.75f;

	// ── 애니메이션 마커 위젯 풀링 옵션 ───────────────────────────────────────
	// WBP_Compass 안에 같은 이름의 CanvasPanel을 만들고 Is Variable 체크.
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UCanvasPanel> CanvasPanel_CompassMarkers;

	// true면 아래에 WidgetClass가 지정된 타입만 NativePaint 대신 실제 위젯으로 표시.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Compass|MarkerWidgets")
	bool bEnableMarkerWidgetPooling = true;

	// Enemy 마커용 애니메이션 위젯.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Compass|MarkerWidgets")
	TSubclassOf<UUserWidget> EnemyMarkerWidgetClass;

	// 에너미로 취급할 아이콘 타입 집합. 라이브 GetIcons()(미니맵과 동일 소스) 기준.
	// 향후 ERetrieveMapIconType에 Enemy enum이 추가되면 여기 넣으면 된다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Compass|MarkerWidgets")
	TSet<ERetrieveMapIconType> EnemyIconTypes = { ERetrieveMapIconType::Boss, ERetrieveMapIconType::Enemy };

	// 0 이하 = 거리 무제한. 양수면 이 반경(UU) 밖 에너미는 나침반에서 숨김.
	// 미니맵 ViewWorldRadius(기본 3000)에 맞추면 두 위젯 표시 범위가 일치.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Compass|MarkerWidgets",
		meta=(ClampMin="0.0"))
	float EnemyViewRadius = 0.0f;

	// 라이브 에너미 마커 기본 크기(픽셀). IconRegistry Row.IconSize가 있으면 그쪽 우선.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Compass|MarkerWidgets",
		meta=(ClampMin="4.0"))
	float EnemyMarkerSize = 16.0f;

	// 사용자 지정 목표 마커(Objective 아이콘)용 애니메이션 위젯.
	// 여기에 Map_FantasyWarrior_Icon_Objective_01 위젯을 지정한다.
	// 비워두면 기존 NativePaint 웨이포인트 마커를 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Compass|MarkerWidgets")
	TSubclassOf<UUserWidget> UserWaypointMarkerWidgetClass;

	// 위젯 마커를 나침반 띠 기준으로 위/아래 보정.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Compass|MarkerWidgets")
	FVector2D WidgetMarkerOffset = FVector2D(0.0f, -10.0f);

	// 유저 웨이포인트 위젯 위치 보정.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Compass|MarkerWidgets")
	FVector2D WaypointWidgetMarkerOffset = FVector2D(0.0f, 0.0f);

protected:
	// 입력 차단 안 함 — HUD 위에 그리기만 함
	virtual bool NativeIsInteractable() const override { return false; }
	virtual bool NativeSupportsKeyboardFocus() const override { return false; }

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

private:
	/**
	 * 방위각(bearingDeg) → 나침반 띠 X 위치.
	 * CameraYaw를 기준으로 정규화: 정면 = CenterX.
	 * 범위 밖이면 -1 반환.
	 */
	float BearingToX(float BearingDeg, float CameraYaw, float CompassWidth) const;

	void DrawCompassText(
		FSlateWindowElementList& OutDrawElements,
		int32& LayerId,
		const FGeometry& AllottedGeometry,
		const FString& Text,
		const FVector2D& CenterPos,
		const FSlateFontInfo& Font,
		const FLinearColor& Color
	) const;

	bool HasUsableMarkerPanel() const;
	bool ShouldUseWidgetMarker(ERetrieveMapIconType IconType) const;
	TSubclassOf<UUserWidget> GetWidgetClassForIconType(ERetrieveMapIconType IconType) const;

	void UpdateWidgetMarkers(const FGeometry& MyGeometry);
	UUserWidget* GetOrCreatePooledMarker(int32 MarkerKey, TSubclassOf<UUserWidget> WidgetClass);
	void SetPooledMarkerTransform(UUserWidget* MarkerWidget, const FVector2D& CenterPos, const FVector2D& Size) const;
	void CollapseUnusedMarkers(const TSet<int32>& ActiveMarkerKeys);

	// 라이브 에너미 컴포넌트에 안정적인 풀 키를 발급/조회한다.
	// 같은 컴포넌트는 항상 같은 키 → 같은 풀 위젯 → 애니메이션 상태 유지.
	int32 GetStableEnemyMarkerKey(const URetrieveMapIconComponent* Icon);

	UPROPERTY()
	TMap<int32, TObjectPtr<UUserWidget>> MarkerWidgetPool;

	// 에너미 컴포넌트(FObjectKey) → 안정 슬롯 번호. UUserWidget을 담지 않으므로 GC 추적 불필요.
	TMap<FObjectKey, int32> EnemyIconSlots;
	int32 NextEnemySlot = 0;
};
