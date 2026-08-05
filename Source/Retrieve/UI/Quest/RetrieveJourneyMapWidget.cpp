#include "UI/Quest/RetrieveJourneyMapWidget.h"

#include "Core/RetrieveGameState.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Fonts/FontMeasure.h"
#include "Quest/QuestBranchComponent.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "UI/Quest/RetrieveQuestStatus.h"
#include "UI/RetrieveUISettingsLibrary.h"

void URetrieveJourneyMapWidget::BuildNodes(TArray<FJourneyNode>& OutNodes, int32& OutColumnCount) const
{
	OutNodes.Reset();
	OutColumnCount = 0;

	const UWorld* World = GetWorld();
	ARetrieveGameState* GS = World ? World->GetGameState<ARetrieveGameState>() : nullptr;
	UQuestBranchComponent* Branch = GS ? GS->GetQuestBranchComponent() : nullptr;
	const UDataTable* Table = GS ? GS->GetQuestTable() : nullptr;
	if (!Branch || !Table)
	{
		return;
	}

	static const FString Ctx(TEXT("JourneyMap"));
	TArray<FQuestDefinition*> Rows;
	Table->GetAllRows<FQuestDefinition>(Ctx, Rows);

	for (const FQuestDefinition* Row : Rows)
	{
		if (!Row || Row->Type != EQuestType::Main)
		{
			continue; // 여정 화면은 메인 줄기만 보여준다(사이드까지 넣으면 다시 복잡해진다).
		}

		FJourneyNode Node;
		Node.DisplayName = Row->DisplayName;
		Node.QuestId = Row->QuestId;
		Node.Unlocks = Row->UnlockRequirements;
		Node.bCompleted = QuestStatus::AreAllObjectivesComplete(*Row, *Branch);
		Node.bActive = !Node.bCompleted && QuestStatus::IsQuestUnlocked(*Row, *Branch);
		OutNodes.Add(MoveTemp(Node));
	}

	if (OutNodes.Num() == 0)
	{
		return;
	}

	// 열(진행 깊이) = 선행 조건 개수 기준의 위상 정렬 근사.
	// 선행 스텝을 완료시키는 퀘스트를 찾아 그 열 + 1로 밀어내며, 사이클이 없으므로 몇 번만 돌면 수렴한다.
	auto FindProviderIndex = [&OutNodes, &Rows](const FGameplayTag& StepTag) -> int32
	{
		for (int32 Index = 0; Index < OutNodes.Num(); ++Index)
		{
			for (const FQuestDefinition* Row : Rows)
			{
				if (!Row || !Row->QuestId.MatchesTagExact(OutNodes[Index].QuestId))
				{
					continue;
				}
				for (const FQuestObjective& Objective : Row->Objectives)
				{
					if (Objective.CompletionTag.MatchesTagExact(StepTag))
					{
						return Index;
					}
				}
			}
		}
		return INDEX_NONE;
	};

	for (int32 Pass = 0; Pass < OutNodes.Num(); ++Pass)
	{
		bool bChanged = false;
		for (FJourneyNode& Node : OutNodes)
		{
			for (const FGameplayTag& Req : Node.Unlocks)
			{
				const int32 ProviderIndex = FindProviderIndex(Req);
				if (ProviderIndex != INDEX_NONE && OutNodes[ProviderIndex].Column + 1 > Node.Column)
				{
					Node.Column = OutNodes[ProviderIndex].Column + 1;
					bChanged = true;
				}
			}
		}
		if (!bChanged)
		{
			break;
		}
	}

	// 같은 열에 여러 개(수호자 3종)면 행으로 나눠 쌓는다.
	TMap<int32, int32> RowCursor;
	for (FJourneyNode& Node : OutNodes)
	{
		// 지역변수명이 UUserWidget::Cursor를 가리면 경고→에러가 되므로 이름을 피한다.
		int32& NextRowInColumn = RowCursor.FindOrAdd(Node.Column);
		Node.Row = NextRowInColumn++;
		OutColumnCount = FMath::Max(OutColumnCount, Node.Column + 1);
	}

	OutNodes.Sort([](const FJourneyNode& A, const FJourneyNode& B)
	{
		return A.Column != B.Column ? A.Column < B.Column : A.Row < B.Row;
	});
}

int32 URetrieveJourneyMapWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	int32 CurrentLayer = LayerId;

	TArray<FJourneyNode> Nodes;
	int32 ColumnCount = 0;
	BuildNodes(Nodes, ColumnCount);
	if (Nodes.Num() == 0 || ColumnCount == 0)
	{
		return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements,
			CurrentLayer, InWidgetStyle, bParentEnabled);
	}

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const float Margin = NodeSize * 1.5f;
	const float UsableWidth = FMath::Max(Size.X - Margin * 2.0f, 1.0f);
	const float ColumnSpacing = ColumnCount > 1
		? FMath::Max(UsableWidth / (ColumnCount - 1), MinColumnSpacing * 0.5f)
		: 0.0f;

	int32 MaxRow = 0;
	for (const FJourneyNode& Node : Nodes)
	{
		MaxRow = FMath::Max(MaxRow, Node.Row);
	}
	const float RowSpacing = MaxRow > 0
		? FMath::Min((Size.Y - Margin * 2.0f) / MaxRow, NodeSize * 3.0f)
		: 0.0f;

	auto NodeCenter = [&](const FJourneyNode& Node)
	{
		const float X = Margin + Node.Column * ColumnSpacing;
		const float Y = Size.Y * 0.5f + (Node.Row - MaxRow * 0.5f) * RowSpacing;
		return FVector2D(X, Y);
	};

	// 1) 연결선을 먼저 깔아 노드가 위에 오게 한다.
	for (int32 i = 0; i < Nodes.Num(); ++i)
	{
		for (int32 j = 0; j < Nodes.Num(); ++j)
		{
			if (Nodes[j].Column != Nodes[i].Column + 1)
			{
				continue;
			}

			TArray<FVector2D> Line;
			Line.Add(NodeCenter(Nodes[i]));
			Line.Add(NodeCenter(Nodes[j]));

			FSlateDrawElement::MakeLines(
				OutDrawElements,
				++CurrentLayer,
				AllottedGeometry.ToPaintGeometry(),
				Line,
				ESlateDrawEffect::None,
				ConnectorColor,
				true,
				1.5f);
		}
	}

	// 2) 노드 + 이름
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(
		"Bold", FMath::RoundToInt(LabelFontSize * URetrieveUISettingsLibrary::GetUIScale()));

	for (const FJourneyNode& Node : Nodes)
	{
		const FVector2D Center = NodeCenter(Node);

		FLinearColor Color = LockedColor;
		float DrawSize = NodeSize;
		if (Node.bCompleted)
		{
			Color = CompletedColor;
		}
		else if (Node.bActive)
		{
			Color = ActiveColor;
			DrawSize = NodeSize * 1.35f; // 진행 중 단계는 크게 — 시선이 여기로 간다.
		}

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			++CurrentLayer,
			AllottedGeometry.ToPaintGeometry(
				FVector2f(DrawSize, DrawSize),
				FSlateLayoutTransform(FVector2f(Center - FVector2D(DrawSize * 0.5f)))),
			FCoreStyle::Get().GetBrush("WhiteBrush"),
			ESlateDrawEffect::None,
			Color);

		DrawLabel(OutDrawElements, CurrentLayer, AllottedGeometry,
			Node.DisplayName.ToString(),
			FVector2D(Center.X, Center.Y + DrawSize * 0.5f + 6.0f),
			Font,
			Node.bActive ? ActiveColor : LabelColor);
	}

	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements,
		CurrentLayer, InWidgetStyle, bParentEnabled);
}

void URetrieveJourneyMapWidget::DrawLabel(
	FSlateWindowElementList& OutDrawElements,
	int32& LayerId,
	const FGeometry& AllottedGeometry,
	const FString& Text,
	const FVector2D& CenterPos,
	const FSlateFontInfo& Font,
	const FLinearColor& Color) const
{
	const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FVector2D TextSize = FontMeasure->Measure(Text, Font);
	const FVector2D DrawPos(CenterPos.X - TextSize.X * 0.5f, CenterPos.Y);

	// 드롭섀도우 → 배경이 밝아도 읽힌다.
	FSlateDrawElement::MakeText(
		OutDrawElements,
		++LayerId,
		AllottedGeometry.ToPaintGeometry(FVector2f(TextSize), FSlateLayoutTransform(FVector2f(DrawPos + FVector2D(1.0f)))),
		Text,
		Font,
		ESlateDrawEffect::None,
		FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));

	FSlateDrawElement::MakeText(
		OutDrawElements,
		++LayerId,
		AllottedGeometry.ToPaintGeometry(FVector2f(TextSize), FSlateLayoutTransform(FVector2f(DrawPos))),
		Text,
		Font,
		ESlateDrawEffect::None,
		Color);
}
