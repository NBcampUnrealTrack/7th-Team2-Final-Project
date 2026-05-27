#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Data/RetrieveMapIconRegistry.h"
#include "UI/RetrieveGamePanelWidget.h"
#include "RetrieveWorldMapWidget.generated.h"

class URetrieveMapSubsystem;
class URetrieveMapIconComponent;
class UTexture2D;
class UWidget;

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
	// 탑뷰 베이크 텍스처 (UV 규약: U=동쪽, V=0=북쪽)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap")
	TObjectPtr<UTexture2D> BakedMapTexture;

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap|Layout")
	FMargin MapViewportPadding = FMargin(24.0f, 72.0f, 24.0f, 24.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap|Layout")
	bool bStretchMapToViewport = true;

	// 1 = 맵 전체 표시, 값이 클수록 확대
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap", meta=(ClampMin="1.0"))
	float MinZoom = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap", meta=(ClampMin="1.0"))
	float MaxZoom = 8.0f;

	// 마우스 휠 1틱당 줌 변화량
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|WorldMap", meta=(ClampMin="0.1"))
	float ZoomStep = 0.5f;

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

protected:
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> MapViewport;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> HorizontalBox_Body;

	// 마우스 휠·클릭 입력 수신을 위한 필수 오버라이드
	virtual bool NativeIsInteractable() const override { return true; }
	virtual bool NativeSupportsKeyboardFocus() const override { return true; }

	virtual void NativeConstruct() override;
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
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	float     ZoomLevel    = 1.0f;
	FVector2D ViewCenterUV = FVector2D(0.5f, 0.5f);

	bool      bIsPanning       = false;
	bool      bPendingCenterOnPlayer = true;
	FVector2D PanStartLocalPos;  // 드래그 시작 위젯 로컬 좌표
	FVector2D PanStartCenterUV;  // 드래그 시작 ViewCenterUV

	// ViewCenterUV를 맵 범위 내로 클램핑
	void ClampViewCenter();

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

	void GetMapViewRect(const FGeometry& AllottedGeometry, FVector2D& OutTopLeft, FVector2D& OutSize) const;
	bool IsInsideMapView(const FVector2D& LocalPosition, const FVector2D& MapTopLeft, const FVector2D& MapSize) const;
	APlayerController* GetWorldMapPlayerController() const;

	void DrawWorldIcon(
		FSlateWindowElementList& OutDrawElements,
		int32& LayerId,
		const FGeometry& AllottedGeometry,
		const URetrieveMapIconComponent* Icon,
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
