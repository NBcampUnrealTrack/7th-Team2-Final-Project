#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/RetrieveObjectiveMarkerTypes.h"
#include "RetrieveObjectiveMarkerWidget.generated.h"

class UImage;
class UTextBlock;

/** 마커 위젯 1개에 매 갱신마다 전달되는 표시 데이터. */
USTRUCT(BlueprintType)
struct FRetrieveObjectiveMarkerVisual
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|ObjectiveMarker")
	ERetrieveObjectiveMarkerKind Kind = ERetrieveObjectiveMarkerKind::Side;

	/** 퀘스트 제목/목표 문구. bShowLabel이 false면 숨긴다. */
	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|ObjectiveMarker")
	FText Label;

	/** "남은 적 3" 같은 진행 문구. */
	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|ObjectiveMarker")
	FText ProgressText;

	/** 플레이어 ~ 목표 거리(m). 첨부 이미지의 "12m" 표기에 사용. */
	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|ObjectiveMarker")
	float DistanceMeters = 0.0f;

	/** 목표가 화면 밖이라 가장자리로 클램프된 상태. */
	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|ObjectiveMarker")
	bool bOffscreen = false;

	/** 화면 밖일 때 방향 화살표에 적용할 각도(도). 0 = 오른쪽. */
	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|ObjectiveMarker")
	float EdgeAngleDeg = 0.0f;

	/** 중거리 이내라 라벨을 보여줄 단계인지. */
	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|ObjectiveMarker")
	bool bShowLabel = false;

	/** 위치가 아직 지역 단위(정밀 스냅 전)인지. */
	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|ObjectiveMarker")
	bool bApproximate = false;

	/** 근접 페이드 등을 반영한 최종 알파. */
	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|ObjectiveMarker")
	float Opacity = 1.0f;

	/** 등장 연출 진행도(0=방금 생김, 1=정착). 펄스/스케일 인에 사용. */
	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|ObjectiveMarker")
	float AppearProgress = 1.0f;

	/** 거리/등장/호흡 펄스를 합친 최종 스케일 배율. */
	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|ObjectiveMarker")
	float ScaleMultiplier = 1.0f;

	/** 지형/구조물에 가려져 있는지. 가려지면 옅게 그려 "벽 너머"임을 알린다. */
	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|ObjectiveMarker")
	bool bOccluded = false;
};

/**
 * 화면에 떠 있는 목표 마커 1개(WBP_ObjectiveMarker의 부모).
 *
 * WBP 구성(전부 선택 — 있는 것만 자동 갱신된다):
 *   Image_Icon     : 다이아 아이콘. Kind별 색이 적용된다.
 *   Image_Arrow    : 화면 밖 방향 화살표. 화면 안이면 자동으로 숨겨진다.
 *   Text_Distance  : "12m"
 *   Text_Label     : 퀘스트 제목
 *   Text_Progress  : "남은 적 3"
 *
 * 추가 연출이 필요하면 OnMarkerUpdated / OnMarkerAppeared 이벤트를 BP에서 구현한다.
 */
UCLASS(Abstract)
class RETRIEVE_API URetrieveObjectiveMarkerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 레이어 위젯이 매 갱신마다 호출. C++ 기본 갱신 후 BP 이벤트를 발생시킨다. */
	void ApplyVisual(const FRetrieveObjectiveMarkerVisual& InVisual);

	/** 마커가 처음 생성됐을 때 1회(퀘스트 수락 순간의 등장 연출용). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Retrieve|ObjectiveMarker")
	void OnMarkerAppeared(ERetrieveObjectiveMarkerKind Kind);

	/** 매 갱신. 기본 텍스트/색 처리는 C++이 이미 끝낸 뒤 호출된다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Retrieve|ObjectiveMarker")
	void OnMarkerUpdated(const FRetrieveObjectiveMarkerVisual& Visual);

	/** 종류별 색. 메인=금색, 인스턴스=빨강, 보상 수령=초록. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker|Style")
	FLinearColor MainColor = FLinearColor(1.0f, 0.82f, 0.25f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker|Style")
	FLinearColor SideColor = FLinearColor(1.0f, 0.28f, 0.24f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker|Style")
	FLinearColor TurnInColor = FLinearColor(0.42f, 0.92f, 0.45f, 1.0f);

	/** 아직 수락하지 않은 의뢰. 인스턴스 계열이되 "미수락"이 드러나도록 주황 계열. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker|Style")
	FLinearColor OfferColor = FLinearColor(1.0f, 0.62f, 0.18f, 1.0f);

	/** 메인 퀘스트의 선택 목표. 금색 계열이되 채도를 낮춰 "필수는 아니다"를 드러낸다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker|Style")
	FLinearColor MainOptionalColor = FLinearColor(0.85f, 0.80f, 0.55f, 1.0f);

	/**
	 * 종류별 아이콘 텍스처(선택).
	 * WBP에 `Image_KindIcon`(UImage)을 두면 종류에 맞는 텍스처로 교체되고,
	 * 해당 종류의 텍스처가 지정되지 않았거나 위젯이 없으면 기본 다이아 아이콘만 쓴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker|Style")
	TMap<ERetrieveObjectiveMarkerKind, TObjectPtr<UTexture2D>> KindIcons;

	/** 위치가 지역 단위일 때 아이콘에 곱하는 알파(살짝 흐리게 = "대략 이 근처"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker|Style",
		meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float ApproximateAlphaScale = 0.65f;

	/** 지형에 가려졌을 때 곱하는 알파. "저 너머에 있다"를 알리되 시야를 방해하지 않는 값. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker|Style",
		meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float OccludedAlphaScale = 0.45f;

	/**
	 * 마커가 처음 생길 때 재생할 위젯 애니메이션 이름.
	 * WBP_ObjectiveMarker는 원본 에셋의 "Loop" 애니메이션을 그대로 갖고 있다.
	 * 비우면 C++ 스케일 인 연출만 사용한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker|Style")
	FName AppearAnimationName = TEXT("Loop");

	/** 등장 애니메이션 재생 횟수. 0이면 무한 반복. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker|Style", meta = (ClampMin = "0"))
	int32 AppearAnimationLoops = 2;

	/** 등장 시 C++ 스케일 인 연출을 재생한다(애니메이션 유무와 무관). */
	void PlayAppearEffect();

	UFUNCTION(BlueprintPure, Category = "Retrieve|ObjectiveMarker")
	FLinearColor GetColorForKind(ERetrieveObjectiveMarkerKind Kind) const;

protected:
	virtual bool NativeIsInteractable() const override { return false; }
	virtual bool NativeSupportsKeyboardFocus() const override { return false; }

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Icon;

	/** 종류별 아이콘을 그릴 이미지(선택). KindIcons에 텍스처가 있을 때만 표시된다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_KindIcon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Arrow;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Distance;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Label;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Progress;
};
