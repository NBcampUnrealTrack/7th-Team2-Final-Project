#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Styling/SlateBrush.h"
#include "UI/Puzzle/RetrievePuzzleTypes.h"
#include "RetrievePuzzleBoardWidget.generated.h"

class UTexture2D;
class USoundBase;

// 보드의 영역 구성이 바뀔 때마다 브로드캐스트 (드래그 1스텝 단위).
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRetrievePuzzleBoardChangedSignature);
// 정답 여부가 바뀔 때 브로드캐스트.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRetrievePuzzleSolvedSignature, bool, bIsSolved);

/**
 * 격자 영역 분할 퍼즐 보드 위젯 (커스텀 페인트).
 *
 * 규칙:
 *   - 보드를 여러 영역으로 분할 → 각 영역이 "보드에 존재하는 모든 원소 종류를
 *     정확히 하나씩" 포함하면 클리어.
 *
 * 조작:
 *   - 좌클릭 드래그: 빈 칸에서 시작하면 새 영역 생성, 칠해진 칸에서 시작하면 그 영역 확장.
 *     드래그 경로의 인접 칸이 활성 영역에 합류(다른 영역 칸은 빼앗아 옴).
 *   - 우클릭 드래그: 지우개(칸을 미할당으로).
 *
 * WBP 설정:
 *   - 이 클래스로 WBP를 만들고 화면에 배치. (Visibility는 런타임에 Visible로 강제됨)
 *   - GridWidth / GridHeight / ElementPlacements로 테스트 보드를 구성.
 *   - ElementIcons에 원소별 텍스처를 지정(없으면 원소 색 사각형으로 폴백).
 */
UCLASS()
class RETRIEVE_API URetrievePuzzleBoardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	URetrievePuzzleBoardWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// ---------- 보드 정의 (인라인 테스트용) ----------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Board", meta = (ClampMin = "1"))
	int32 GridWidth = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Board", meta = (ClampMin = "1"))
	int32 GridHeight = 4;

	// 보드에서 제외할 칸(불규칙 보드용 구멍). (X = Col, Y = Row)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Board")
	TArray<FIntPoint> AbsentCells;

	// 원소 배치.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Board")
	TArray<FRetrievePuzzleElementPlacement> ElementPlacements;

	// ---------- 비주얼 ----------

	// 원소 태그별 아이콘. 항목이 없으면 ElementTagToColor 색의 사각형으로 폴백.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Visual")
	TMap<FGameplayTag, TObjectPtr<UTexture2D>> ElementIcons;

	// 영역 색 팔레트(RegionId 순환). 비어 있으면 색상환으로 자동 생성.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Visual")
	TArray<FLinearColor> RegionPalette;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Visual")
	FLinearColor UnassignedCellColor = FLinearColor(0.12f, 0.12f, 0.14f, 1.0f);

	// 보드 배경 바탕 텍스처(브러시). 격자 영역 뒤에 그려진다. 리소스가 없으면 안 그림.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Visual")
	FSlateBrush BackgroundBrush;

	// 칸을 영역에 채웠을 때 그릴 텍스처(브러시). 영역 색으로 틴트됨. 리소스가 없으면 단색으로 채움.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Visual")
	FSlateBrush RegionFillBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Visual")
	FLinearColor GridLineColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.25f);

	// 같은 영역 내부 격자선 두께.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Visual", meta = (ClampMin = "0.5"))
	float GridLineThickness = 1.5f;

	// 같은 영역 내부 격자선을 점선으로 — 대시 길이 / 간격(px).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Visual", meta = (ClampMin = "0.5"))
	float GridDashLength = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Visual", meta = (ClampMin = "0.5"))
	float GridDashGap = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Visual")
	FLinearColor RegionBorderColor = FLinearColor(0.05f, 0.03f, 0.02f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Visual", meta = (ClampMin = "1.0"))
	float RegionBorderThickness = 4.0f;

	// 칸 크기 대비 원소 아이콘 비율.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Visual", meta = (ClampMin = "0.05", ClampMax = "0.95"))
	float ElementIconRatio = 0.6f;

	// ---------- 사운드 ----------

	// 칸이 영역에 채워질 때(드래그로 칠할 때, 칸당 1회). 짧은 효과음 권장.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Audio")
	TObjectPtr<USoundBase> FillSound;

	// 칸이 지워질 때(우클릭 드래그, 칸당 1회). 짧은 효과음 권장.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Audio")
	TObjectPtr<USoundBase> EraseSound;

	// 퍼즐이 처음 풀렸을 때(클리어).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Audio")
	TObjectPtr<USoundBase> SolvedSound;

	// 영역 전체 초기화(리셋 버튼) 시.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Audio")
	TObjectPtr<USoundBase> ResetSound;

	// ---------- 이벤트 ----------

	UPROPERTY(BlueprintAssignable, Category = "Puzzle")
	FRetrievePuzzleBoardChangedSignature OnBoardChanged;

	UPROPERTY(BlueprintAssignable, Category = "Puzzle")
	FRetrievePuzzleSolvedSignature OnSolvedChanged;

	// ---------- API ----------

	// 인라인 정의(GridWidth/Height/AbsentCells/ElementPlacements)로부터 보드를 재생성. 영역은 모두 초기화.
	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	void RebuildBoard();

	// 모든 칸을 미할당 상태로.
	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	void ClearAllRegions();

	// 현재 분할이 정답인지(모든 칸 할당 + 영역별 원소 하나씩 + 영역 연결).
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Puzzle")
	bool IsSolved() const;

	// 현재 사용 중인(셀이 1개 이상인) 영역 개수.
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Puzzle")
	int32 GetRegionCount() const;

	// 외부 보드 정의(생성기 결과 등)를 적용하고 다시 그린다. 영역은 모두 초기화.
	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	void LoadBoardDefinition(const FRetrievePuzzleBoardDef& Definition);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	// 런타임 보드 상태. row-major: index = Row * GridWidth + Col.
	UPROPERTY(Transient)
	TArray<FRetrievePuzzleCell> Cells;

	// 드래그 상태.
	bool bDragging = false;
	bool bErasing = false;
	int32 ActiveRegionId = INDEX_NONE;
	int32 NextRegionId = 0;
	FIntPoint LastPaintedCell = FIntPoint(-1, -1);
	bool bLastSolved = false;

	// --- 셀 접근 헬퍼 ---
	bool IsValidCoord(int32 Col, int32 Row) const;
	int32 CellIndex(int32 Col, int32 Row) const;
	const FRetrievePuzzleCell* GetCell(int32 Col, int32 Row) const;
	FRetrievePuzzleCell* GetCellMutable(int32 Col, int32 Row);

	// --- 좌표 변환 ---
	bool LocalToCell(const FGeometry& Geometry, const FVector2D& LocalPos, FIntPoint& OutCell) const;
	// 정사각 칸 레이아웃: 한 칸 크기(정사각)와 보드를 위젯 중앙에 둘 오프셋을 계산.
	void GetBoardLayout(const FVector2D& WidgetSize, float& OutCell, FVector2D& OutOrigin) const;

	// --- 드래그 처리 ---
	void BeginStroke(const FIntPoint& Cell, bool bErase);
	void ContinueStroke(const FIntPoint& Cell);
	void EndStroke();
	bool IsAdjacentToActiveRegion(const FIntPoint& Cell) const;
	void PaintCellToActiveRegion(const FIntPoint& Cell);
	void EraseCell(const FIntPoint& Cell);

	// --- 기타 ---
	FLinearColor GetRegionColor(int32 RegionId) const;
	void NotifyBoardChanged();
	void UpdateSolvedState();
	void PlayPuzzleSound(USoundBase* Sound) const;
	bool AreRegionsContiguous() const;
};
