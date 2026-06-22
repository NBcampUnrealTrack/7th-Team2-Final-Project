#include "UI/Puzzle/RetrievePuzzleBoardWidget.h"

#include "UI/RetrieveElementUILibrary.h"

#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "InputCoreTypes.h"

URetrievePuzzleBoardWidget::URetrievePuzzleBoardWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 스테인드글라스 느낌의 기본 팔레트.
	RegionPalette = {
		FLinearColor(0.90f, 0.30f, 0.24f),
		FLinearColor(0.96f, 0.62f, 0.20f),
		FLinearColor(0.97f, 0.86f, 0.30f),
		FLinearColor(0.55f, 0.78f, 0.34f),
		FLinearColor(0.30f, 0.70f, 0.55f),
		FLinearColor(0.32f, 0.62f, 0.86f),
		FLinearColor(0.40f, 0.45f, 0.80f),
		FLinearColor(0.62f, 0.42f, 0.78f),
		FLinearColor(0.88f, 0.46f, 0.66f),
		FLinearColor(0.78f, 0.55f, 0.38f),
		FLinearColor(0.45f, 0.75f, 0.78f),
		FLinearColor(0.70f, 0.72f, 0.30f),
	};
}

// ---------------------------------------------------------------------------
// 생성 / 보드 빌드
// ---------------------------------------------------------------------------

void URetrievePuzzleBoardWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	RebuildBoard(); // 디자이너 미리보기
}

void URetrievePuzzleBoardWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// 마우스 입력을 받으려면 hit-test 가능한 Visible 상태여야 한다.
	SetVisibility(ESlateVisibility::Visible);
	RebuildBoard();
}

void URetrievePuzzleBoardWidget::RebuildBoard()
{
	GridWidth = FMath::Max(1, GridWidth);
	GridHeight = FMath::Max(1, GridHeight);

	Cells.Reset();
	Cells.SetNum(GridWidth * GridHeight); // 기본값: ElementTag 없음, RegionId = INDEX_NONE, bExists = true

	for (const FIntPoint& Absent : AbsentCells)
	{
		if (FRetrievePuzzleCell* C = GetCellMutable(Absent.X, Absent.Y))
		{
			C->bExists = false;
		}
	}

	for (const FRetrievePuzzleElementPlacement& Placement : ElementPlacements)
	{
		if (FRetrievePuzzleCell* C = GetCellMutable(Placement.Coord.X, Placement.Coord.Y))
		{
			if (C->bExists)
			{
				C->ElementTag = Placement.ElementTag;
			}
		}
	}

	bDragging = false;
	bErasing = false;
	ActiveRegionId = INDEX_NONE;
	NextRegionId = 0;
	LastPaintedCell = FIntPoint(-1, -1);
	bLastSolved = false;

	NotifyBoardChanged();
}

void URetrievePuzzleBoardWidget::LoadBoardDefinition(const FRetrievePuzzleBoardDef& Definition)
{
	GridWidth = FMath::Max(1, Definition.GridWidth);
	GridHeight = FMath::Max(1, Definition.GridHeight);
	AbsentCells = Definition.AbsentCells;
	ElementPlacements = Definition.ElementPlacements;
	RebuildBoard();
}

void URetrievePuzzleBoardWidget::ClearAllRegions()
{
	for (FRetrievePuzzleCell& C : Cells)
	{
		C.RegionId = INDEX_NONE;
	}
	ActiveRegionId = INDEX_NONE;
	NextRegionId = 0;
	NotifyBoardChanged();
	UpdateSolvedState();
}

// ---------------------------------------------------------------------------
// 셀 접근 헬퍼
// ---------------------------------------------------------------------------

bool URetrievePuzzleBoardWidget::IsValidCoord(int32 Col, int32 Row) const
{
	return Col >= 0 && Col < GridWidth && Row >= 0 && Row < GridHeight;
}

int32 URetrievePuzzleBoardWidget::CellIndex(int32 Col, int32 Row) const
{
	return Row * GridWidth + Col;
}

const FRetrievePuzzleCell* URetrievePuzzleBoardWidget::GetCell(int32 Col, int32 Row) const
{
	if (!IsValidCoord(Col, Row) || Cells.Num() != GridWidth * GridHeight)
	{
		return nullptr;
	}
	return &Cells[CellIndex(Col, Row)];
}

FRetrievePuzzleCell* URetrievePuzzleBoardWidget::GetCellMutable(int32 Col, int32 Row)
{
	if (!IsValidCoord(Col, Row) || Cells.Num() != GridWidth * GridHeight)
	{
		return nullptr;
	}
	return &Cells[CellIndex(Col, Row)];
}

// ---------------------------------------------------------------------------
// 마우스 입력
// ---------------------------------------------------------------------------

void URetrievePuzzleBoardWidget::GetBoardLayout(const FVector2D& WidgetSize, float& OutCell, FVector2D& OutOrigin) const
{
	if (GridWidth <= 0 || GridHeight <= 0)
	{
		OutCell = 0.0f;
		OutOrigin = FVector2D::ZeroVector;
		return;
	}
	// 칸을 정사각형으로: 더 빡빡한 축에 맞춰 한 변을 정하고, 보드를 위젯 중앙에 배치한다.
	OutCell = FMath::Min(WidgetSize.X / GridWidth, WidgetSize.Y / GridHeight);
	const float BoardW = OutCell * GridWidth;
	const float BoardH = OutCell * GridHeight;
	OutOrigin = FVector2D((WidgetSize.X - BoardW) * 0.5f, (WidgetSize.Y - BoardH) * 0.5f);
}

bool URetrievePuzzleBoardWidget::LocalToCell(const FGeometry& Geometry, const FVector2D& LocalPos, FIntPoint& OutCell) const
{
	const FVector2D Size = Geometry.GetLocalSize();
	if (Size.X <= 0.0 || Size.Y <= 0.0 || GridWidth <= 0 || GridHeight <= 0)
	{
		return false;
	}

	float Cell;
	FVector2D Origin;
	GetBoardLayout(Size, Cell, Origin);
	if (Cell <= 0.0f)
	{
		return false;
	}

	const int32 Col = FMath::FloorToInt((LocalPos.X - Origin.X) / Cell);
	const int32 Row = FMath::FloorToInt((LocalPos.Y - Origin.Y) / Cell);
	if (!IsValidCoord(Col, Row))
	{
		return false;
	}

	OutCell = FIntPoint(Col, Row);
	return true;
}

FReply URetrievePuzzleBoardWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FKey Button = InMouseEvent.GetEffectingButton();
	const bool bLeft = Button == EKeys::LeftMouseButton;
	const bool bRight = Button == EKeys::RightMouseButton;
	if (!bLeft && !bRight)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	const FVector2D Local = FVector2D(InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()));
	FIntPoint Cell;
	if (!LocalToCell(InGeometry, Local, Cell))
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	const FRetrievePuzzleCell* C = GetCell(Cell.X, Cell.Y);
	if (!C || !C->bExists)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	BeginStroke(Cell, bRight);
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply URetrievePuzzleBoardWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bDragging)
	{
		return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	}

	const FVector2D Local = FVector2D(InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()));
	FIntPoint Cell;
	if (LocalToCell(InGeometry, Local, Cell))
	{
		ContinueStroke(Cell);
	}
	return FReply::Handled();
}

FReply URetrievePuzzleBoardWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging)
	{
		EndStroke();
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

// ---------------------------------------------------------------------------
// 드래그 스트로크
// ---------------------------------------------------------------------------

void URetrievePuzzleBoardWidget::BeginStroke(const FIntPoint& Cell, bool bErase)
{
	bDragging = true;
	bErasing = bErase;
	LastPaintedCell = FIntPoint(-1, -1);

	if (bErase)
	{
		ActiveRegionId = INDEX_NONE;
		EraseCell(Cell);
	}
	else
	{
		// 칠해진 칸에서 시작하면 그 영역을 확장, 빈 칸이면 새 영역 생성.
		const FRetrievePuzzleCell* C = GetCell(Cell.X, Cell.Y);
		const int32 Existing = C ? C->RegionId : INDEX_NONE;
		if (Existing != INDEX_NONE)
		{
			ActiveRegionId = Existing;
		}
		else
		{
			ActiveRegionId = NextRegionId++;
			PaintCellToActiveRegion(Cell);
		}
	}

	LastPaintedCell = Cell;
	NotifyBoardChanged();
}

void URetrievePuzzleBoardWidget::ContinueStroke(const FIntPoint& Cell)
{
	if (Cell == LastPaintedCell)
	{
		return;
	}

	const FRetrievePuzzleCell* C = GetCell(Cell.X, Cell.Y);
	if (!C || !C->bExists)
	{
		return;
	}

	if (bErasing)
	{
		EraseCell(Cell);
		LastPaintedCell = Cell;
		NotifyBoardChanged();
		return;
	}

	// 영역은 항상 연결된 형태로만 자라게 한다.
	if (!IsAdjacentToActiveRegion(Cell))
	{
		return;
	}

	PaintCellToActiveRegion(Cell);
	LastPaintedCell = Cell;
	NotifyBoardChanged();
}

void URetrievePuzzleBoardWidget::EndStroke()
{
	bDragging = false;
	bErasing = false;
	ActiveRegionId = INDEX_NONE;
	LastPaintedCell = FIntPoint(-1, -1);
	UpdateSolvedState();
}

bool URetrievePuzzleBoardWidget::IsAdjacentToActiveRegion(const FIntPoint& Cell) const
{
	if (const FRetrievePuzzleCell* Self = GetCell(Cell.X, Cell.Y))
	{
		if (Self->RegionId == ActiveRegionId)
		{
			return true;
		}
	}

	static const FIntPoint Dirs[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
	for (const FIntPoint& D : Dirs)
	{
		const FRetrievePuzzleCell* N = GetCell(Cell.X + D.X, Cell.Y + D.Y);
		if (N && N->bExists && N->RegionId == ActiveRegionId)
		{
			return true;
		}
	}
	return false;
}

void URetrievePuzzleBoardWidget::PaintCellToActiveRegion(const FIntPoint& Cell)
{
	if (FRetrievePuzzleCell* C = GetCellMutable(Cell.X, Cell.Y))
	{
		if (C->bExists)
		{
			C->RegionId = ActiveRegionId;
		}
	}
}

void URetrievePuzzleBoardWidget::EraseCell(const FIntPoint& Cell)
{
	if (FRetrievePuzzleCell* C = GetCellMutable(Cell.X, Cell.Y))
	{
		C->RegionId = INDEX_NONE;
	}
}

// ---------------------------------------------------------------------------
// 정답 판정
// ---------------------------------------------------------------------------

bool URetrievePuzzleBoardWidget::IsSolved() const
{
	if (GridWidth <= 0 || Cells.Num() != GridWidth * GridHeight)
	{
		return false;
	}

	// 존재하는 칸은 모두 영역에 할당돼야 하고, 등장 원소 집합을 모은다.
	TSet<FGameplayTag> PresentElements;
	bool bAnyExist = false;
	for (const FRetrievePuzzleCell& C : Cells)
	{
		if (!C.bExists)
		{
			continue;
		}
		bAnyExist = true;
		if (C.RegionId == INDEX_NONE)
		{
			return false;
		}
		if (C.ElementTag.IsValid())
		{
			PresentElements.Add(C.ElementTag);
		}
	}
	if (!bAnyExist || PresentElements.Num() == 0)
	{
		return false;
	}

	// 영역별 원소 카운트.
	TMap<int32, TMap<FGameplayTag, int32>> RegionElementCounts;
	TSet<int32> RegionIds;
	for (const FRetrievePuzzleCell& C : Cells)
	{
		if (!C.bExists)
		{
			continue;
		}
		RegionIds.Add(C.RegionId);
		if (C.ElementTag.IsValid())
		{
			RegionElementCounts.FindOrAdd(C.RegionId).FindOrAdd(C.ElementTag)++;
		}
	}

	// 각 영역은 등장 원소를 정확히 하나씩 가져야 한다.
	for (const int32 RegionId : RegionIds)
	{
		const TMap<FGameplayTag, int32>* Counts = RegionElementCounts.Find(RegionId);
		for (const FGameplayTag& Tag : PresentElements)
		{
			const int32 Count = Counts ? Counts->FindRef(Tag) : 0;
			if (Count != 1)
			{
				return false;
			}
		}
	}

	// 각 영역은 4방향으로 연결돼 있어야 한다.
	return AreRegionsContiguous();
}

bool URetrievePuzzleBoardWidget::AreRegionsContiguous() const
{
	// 영역별 전체 셀 수.
	TMap<int32, int32> RegionTotal;
	for (const FRetrievePuzzleCell& C : Cells)
	{
		if (C.bExists && C.RegionId != INDEX_NONE)
		{
			RegionTotal.FindOrAdd(C.RegionId)++;
		}
	}

	TSet<int32> Checked;
	TArray<bool> Visited;
	Visited.Init(false, Cells.Num());

	static const FIntPoint Dirs[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };

	for (int32 Row = 0; Row < GridHeight; ++Row)
	{
		for (int32 Col = 0; Col < GridWidth; ++Col)
		{
			const int32 StartIdx = CellIndex(Col, Row);
			const FRetrievePuzzleCell& C = Cells[StartIdx];
			if (!C.bExists || C.RegionId == INDEX_NONE || Checked.Contains(C.RegionId))
			{
				continue;
			}

			const int32 RegionId = C.RegionId;
			int32 ConnectedCount = 0;
			TArray<int32> Stack;
			Stack.Add(StartIdx);
			Visited[StartIdx] = true;

			while (Stack.Num() > 0)
			{
				const int32 Cur = Stack.Pop();
				++ConnectedCount;
				const int32 CurCol = Cur % GridWidth;
				const int32 CurRow = Cur / GridWidth;

				for (const FIntPoint& D : Dirs)
				{
					const int32 NCol = CurCol + D.X;
					const int32 NRow = CurRow + D.Y;
					if (!IsValidCoord(NCol, NRow))
					{
						continue;
					}
					const int32 NIdx = CellIndex(NCol, NRow);
					if (Visited[NIdx])
					{
						continue;
					}
					const FRetrievePuzzleCell& N = Cells[NIdx];
					if (N.bExists && N.RegionId == RegionId)
					{
						Visited[NIdx] = true;
						Stack.Add(NIdx);
					}
				}
			}

			if (ConnectedCount != RegionTotal.FindRef(RegionId))
			{
				return false; // 같은 RegionId가 분리되어 존재 → 비연결
			}
			Checked.Add(RegionId);
		}
	}

	return true;
}

int32 URetrievePuzzleBoardWidget::GetRegionCount() const
{
	TSet<int32> Ids;
	for (const FRetrievePuzzleCell& C : Cells)
	{
		if (C.bExists && C.RegionId != INDEX_NONE)
		{
			Ids.Add(C.RegionId);
		}
	}
	return Ids.Num();
}

// ---------------------------------------------------------------------------
// 통지 / 색상
// ---------------------------------------------------------------------------

void URetrievePuzzleBoardWidget::NotifyBoardChanged()
{
	OnBoardChanged.Broadcast();
	Invalidate(EInvalidateWidgetReason::Paint);
}

void URetrievePuzzleBoardWidget::UpdateSolvedState()
{
	const bool bNow = IsSolved();
	if (bNow != bLastSolved)
	{
		bLastSolved = bNow;
		OnSolvedChanged.Broadcast(bNow);
	}
}

FLinearColor URetrievePuzzleBoardWidget::GetRegionColor(int32 RegionId) const
{
	if (RegionId == INDEX_NONE)
	{
		return UnassignedCellColor;
	}
	if (RegionPalette.Num() > 0)
	{
		return RegionPalette[RegionId % RegionPalette.Num()];
	}
	// 팔레트가 없으면 황금각으로 색상환을 돌며 자동 생성.
	const float Hue = FMath::Fmod(RegionId * 137.50776f, 360.0f);
	return FLinearColor::MakeFromHSV8(static_cast<uint8>(Hue / 360.0f * 255.0f), 160, 205);
}

// ---------------------------------------------------------------------------
// 렌더링
// ---------------------------------------------------------------------------

int32 URetrievePuzzleBoardWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(
		Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const FVector2D WidgetSize = AllottedGeometry.GetLocalSize();
	if (WidgetSize.X <= 1.0 || WidgetSize.Y <= 1.0 || GridWidth <= 0 || GridHeight <= 0
		|| Cells.Num() != GridWidth * GridHeight)
	{
		return BaseLayer;
	}

	float Cell;
	FVector2D Origin;
	GetBoardLayout(WidgetSize, Cell, Origin);
	if (Cell <= 0.0f)
	{
		return BaseLayer;
	}
	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));

	const int32 FillLayer = BaseLayer + 1;
	const int32 IconLayer = BaseLayer + 2;
	const int32 LineLayer = BaseLayer + 3;

	auto DrawBox = [&](const FVector2D& Pos, const FVector2D& BoxSize, const FSlateBrush* Brush, const FLinearColor& Tint, int32 Layer)
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			Layer,
			AllottedGeometry.ToPaintGeometry(FVector2f(BoxSize), FSlateLayoutTransform(FVector2f(Pos))),
			Brush,
			ESlateDrawEffect::None,
			Tint);
	};

	auto DrawEdge = [&](const FVector2D& A, const FVector2D& B, const FLinearColor& Color, float Thickness)
	{
		TArray<FVector2D> Points;
		Points.Add(A);
		Points.Add(B);
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LineLayer,
			AllottedGeometry.ToPaintGeometry(),
			Points,
			ESlateDrawEffect::None,
			Color,
			true,
			Thickness);
	};

	// 1) 셀 채움
	for (int32 Row = 0; Row < GridHeight; ++Row)
	{
		for (int32 Col = 0; Col < GridWidth; ++Col)
		{
			const FRetrievePuzzleCell& C = Cells[CellIndex(Col, Row)];
			if (!C.bExists)
			{
				continue;
			}
			const FVector2D Pos(Origin.X + Col * Cell, Origin.Y + Row * Cell);
			const FLinearColor Fill = (C.RegionId == INDEX_NONE) ? UnassignedCellColor : GetRegionColor(C.RegionId);
			DrawBox(Pos, FVector2D(Cell, Cell), WhiteBrush, Fill, FillLayer);
		}
	}

	// 2) 원소 아이콘
	for (int32 Row = 0; Row < GridHeight; ++Row)
	{
		for (int32 Col = 0; Col < GridWidth; ++Col)
		{
			const FRetrievePuzzleCell& C = Cells[CellIndex(Col, Row)];
			if (!C.bExists || !C.ElementTag.IsValid())
			{
				continue;
			}

			const float IconBox = Cell * ElementIconRatio;
			const FVector2D Center(Origin.X + (Col + 0.5f) * Cell, Origin.Y + (Row + 0.5f) * Cell);

			UTexture2D* Tex = nullptr;
			if (const TObjectPtr<UTexture2D>* Found = ElementIcons.Find(C.ElementTag))
			{
				Tex = *Found;
			}

			if (Tex)
			{
				// 원본 비율 유지하며 IconBox 정사각형 안에 맞춤(contain) — 전체가 보이고 눌리지 않음.
				FVector2D DrawSize(IconBox, IconBox);
				const float TexW = static_cast<float>(Tex->GetSizeX());
				const float TexH = static_cast<float>(Tex->GetSizeY());
				if (TexW > 0.f && TexH > 0.f)
				{
					const float Aspect = TexW / TexH;
					if (Aspect >= 1.f) { DrawSize = FVector2D(IconBox, IconBox / Aspect); } // 가로가 긴 이미지
					else               { DrawSize = FVector2D(IconBox * Aspect, IconBox); } // 세로가 긴 이미지
				}
				const FVector2D IconPos = Center - DrawSize * 0.5f; // 칸 중앙 정렬

				FSlateBrush IconBrush;
				IconBrush.SetResourceObject(Tex);
				IconBrush.ImageSize = DrawSize;
				DrawBox(IconPos, DrawSize, &IconBrush, FLinearColor::White, IconLayer);
			}
			else
			{
				// 텍스처 미지정 → 원소 색 사각형으로 폴백.
				const FVector2D DrawSize(IconBox, IconBox);
				const FVector2D IconPos = Center - DrawSize * 0.5f;
				const FLinearColor ElemColor = URetrieveElementUILibrary::ElementTagToColor(C.ElementTag);
				DrawBox(IconPos, DrawSize, WhiteBrush, ElemColor, IconLayer);
			}
		}
	}

	// 3) 격자선 + 영역 경계선
	for (int32 Row = 0; Row < GridHeight; ++Row)
	{
		for (int32 Col = 0; Col < GridWidth; ++Col)
		{
			const FRetrievePuzzleCell& C = Cells[CellIndex(Col, Row)];
			if (!C.bExists)
			{
				continue;
			}

			const float X0 = Origin.X + Col * Cell;
			const float Y0 = Origin.Y + Row * Cell;
			const float X1 = Origin.X + (Col + 1) * Cell;
			const float Y1 = Origin.Y + (Row + 1) * Cell;

			// 내부 에지는 오른쪽/아래만 그려 중복 방지.
			if (const FRetrievePuzzleCell* RightCell = GetCell(Col + 1, Row))
			{
				if (RightCell->bExists)
				{
					const bool bBorder = (C.RegionId != RightCell->RegionId);
					DrawEdge(FVector2D(X1, Y0), FVector2D(X1, Y1),
						bBorder ? RegionBorderColor : GridLineColor,
						bBorder ? RegionBorderThickness : 1.0f);
				}
			}
			if (const FRetrievePuzzleCell* BottomCell = GetCell(Col, Row + 1))
			{
				if (BottomCell->bExists)
				{
					const bool bBorder = (C.RegionId != BottomCell->RegionId);
					DrawEdge(FVector2D(X0, Y1), FVector2D(X1, Y1),
						bBorder ? RegionBorderColor : GridLineColor,
						bBorder ? RegionBorderThickness : 1.0f);
				}
			}

			// 외곽(이웃이 존재하지 않는 방향)은 굵은 경계.
			auto NeighborMissing = [&](int32 NCol, int32 NRow)
			{
				const FRetrievePuzzleCell* N = GetCell(NCol, NRow);
				return (N == nullptr) || !N->bExists;
			};
			if (NeighborMissing(Col - 1, Row)) { DrawEdge(FVector2D(X0, Y0), FVector2D(X0, Y1), RegionBorderColor, RegionBorderThickness); }
			if (NeighborMissing(Col + 1, Row)) { DrawEdge(FVector2D(X1, Y0), FVector2D(X1, Y1), RegionBorderColor, RegionBorderThickness); }
			if (NeighborMissing(Col, Row - 1)) { DrawEdge(FVector2D(X0, Y0), FVector2D(X1, Y0), RegionBorderColor, RegionBorderThickness); }
			if (NeighborMissing(Col, Row + 1)) { DrawEdge(FVector2D(X0, Y1), FVector2D(X1, Y1), RegionBorderColor, RegionBorderThickness); }
		}
	}

	return LineLayer;
}
