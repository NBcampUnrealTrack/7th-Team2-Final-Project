#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RetrieveMinimapWidget.generated.h"

class UImage;
class UMaterialInstanceDynamic;
class URetrieveMapSubsystem;
class URetrieveMapIconRegistry;
class URetrieveMapIconComponent;

UENUM(BlueprintType)
enum class ERetrieveMinimapRotationMode : uint8
{
	NorthUp  UMETA(DisplayName="North Up"),
	PlayerUp UMETA(DisplayName="Player Up"),
};

/**
 * 정적 베이크 MapTexture 기반 미니맵 HUD 위젯.
 *
 * WBP 설정 체크리스트:
 *   1. Image_Minimap 이름의 UImage 위젯을 루트 아래 배치
 *   2. Image_Minimap의 Brush Material에 M_Minimap 머티리얼 할당
 *   3. BakedMapTexture에 탑뷰 베이크 텍스처 할당
 *   4. IconRegistry에 URetrieveMapIconRegistry 오브젝트 할당 후 DT_MapIconRegistry 연결
 *
 * M_Minimap 머티리얼 파라미터:
 *   - MapTexture  (Texture)  : 베이크 맵 텍스처
 *   - CenterUV    (Vector)   : 플레이어 UV 위치 (R=U, G=V)
 *   - Zoom        (Scalar)   : MapExtent / (ViewWorldRadius * 2)
 *
 * 머티리얼 UV 계산 (머티리얼 그래프에서 구현):
 *   centered = TexCoord - 0.5
 *   finalUV  = CenterUV + centered / Zoom
 *   Output   = MapTexture.Sample(finalUV)
 *
 * PlayerUp 회전은 머티리얼 파라미터가 아니라 NativePaint의 Slate 회전으로 처리한다.
 */
UCLASS()
class RETRIEVE_API URetrieveMinimapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 탑뷰 베이크 텍스처 (UV 규약: U=동쪽, V=0=북쪽)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Minimap")
	TObjectPtr<UTexture2D> BakedMapTexture;

	// 플레이어 마커 텍스처 (위쪽이 북쪽을 가리키는 방향으로 제작)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Minimap")
	TObjectPtr<UTexture2D> PlayerMarkerTexture;

	// DT_MapIconRegistry가 연결된 레지스트리 오브젝트
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Minimap")
	TObjectPtr<URetrieveMapIconRegistry> IconRegistry;

	// 미니맵에 보이는 월드 반경 (UU)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Minimap", meta=(ClampMin="100"))
	float ViewWorldRadius = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Minimap")
	float PlayerMarkerSize = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Minimap")
	FLinearColor PlayerMarkerColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Minimap")
	ERetrieveMinimapRotationMode RotationMode = ERetrieveMinimapRotationMode::NorthUp;

	UFUNCTION(BlueprintCallable, Category="Retrieve|Minimap")
	void ToggleRotationMode();

protected:
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

private:
	// WBP에서 반드시 Image_Minimap 이름의 UImage 위젯을 배치해야 함
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> Image_Minimap;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> MinimapMID;

	void UpdateMinimapMaterial(URetrieveMapSubsystem* MapSub, const FVector& PlayerLocation);

	/**
	 * 아이콘 월드 좌표 → 미니맵 위젯 로컬 좌표.
	 * 월드 공간 직접 계산: 레벨 형태(직사각/정사각)에 무관하게 정확.
	 *   +Y(East)  → 화면 오른쪽  (+ScreenX)
	 *   +X(North) → 화면 위쪽    (-ScreenY)
	 */
	FVector2D WorldToLocal(
		const FVector& TargetWorld,
		const FVector& PlayerWorld,
		const FVector2D& Center,
		const FVector2D& WidgetSize,
		float CameraYaw
	) const;

	FVector2D Rotate2D(const FVector2D& V, float Degrees) const;
	float GetCameraYaw(const APlayerController* PC) const;

	void DrawIcon(
		FSlateWindowElementList& OutDrawElements,
		int32& LayerId,
		const FGeometry& AllottedGeometry,
		const URetrieveMapIconComponent* Icon,
		const FVector2D& Pos
	) const;
};
