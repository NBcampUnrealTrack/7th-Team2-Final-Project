#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Fonts/SlateFontInfo.h"
#include "RetrieveJourneyMapWidget.generated.h"

/**
 * "이게 뭐하는 게임인지" 한 화면에 답하는 여정 다이어그램.
 *
 * DT_Quest의 UnlockRequirements가 이미 퀘스트 간 의존 그래프를 담고 있으므로,
 * 별도 데이터 없이 그것만 읽어 단계 노드를 배치하고 완료/진행/잠김을 색으로 구분한다.
 * (수호자 3종처럼 같은 선행 조건을 가진 퀘스트는 같은 열에 나란히 놓인다)
 *
 * WBP 설정: 이 클래스를 부모로 하는 위젯을 만들어 퀘스트 로그 패널 안에 배치하면 된다.
 * 자식 위젯 없이 NativePaint로 직접 그리므로 별도 바인딩이 필요 없다.
 */
UCLASS()
class RETRIEVE_API URetrieveJourneyMapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 완료한 단계. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Journey|Style")
	FLinearColor CompletedColor = FLinearColor(0.42f, 0.92f, 0.45f, 1.0f);

	/** 지금 진행 중인 단계(강조). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Journey|Style")
	FLinearColor ActiveColor = FLinearColor(1.0f, 0.82f, 0.25f, 1.0f);

	/** 아직 잠긴 단계. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Journey|Style")
	FLinearColor LockedColor = FLinearColor(0.45f, 0.45f, 0.5f, 0.8f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Journey|Style")
	FLinearColor ConnectorColor = FLinearColor(0.6f, 0.6f, 0.65f, 0.6f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Journey|Style")
	FLinearColor LabelColor = FLinearColor(0.95f, 0.93f, 0.88f, 1.0f);

	/** 노드 한 변 크기(픽셀). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Journey|Layout", meta = (ClampMin = "8.0"))
	float NodeSize = 26.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Journey|Layout", meta = (ClampMin = "8"))
	int32 LabelFontSize = 12;

	/** 열(단계) 사이 최소 간격. 위젯 폭이 좁으면 자동으로 줄어든다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Journey|Layout", meta = (ClampMin = "40.0"))
	float MinColumnSpacing = 120.0f;

protected:
	virtual bool NativeIsInteractable() const override { return false; }

	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	/** 다이어그램 노드 1개 = 퀘스트 1개. */
	struct FJourneyNode
	{
		FText DisplayName;
		FGameplayTag QuestId;
		/** 이 퀘스트를 여는 선행 스텝들. 열(깊이) 계산에 사용. */
		TArray<FGameplayTag> Unlocks;
		int32 Column = 0;
		int32 Row = 0;
		bool bCompleted = false;
		bool bActive = false;
	};

	/** DT_Quest + 진행 원장을 읽어 노드를 만들고 열/행을 배치한다. */
	void BuildNodes(TArray<FJourneyNode>& OutNodes, int32& OutColumnCount) const;

	void DrawLabel(
		FSlateWindowElementList& OutDrawElements,
		int32& LayerId,
		const FGeometry& AllottedGeometry,
		const FString& Text,
		const FVector2D& CenterPos,
		const FSlateFontInfo& Font,
		const FLinearColor& Color) const;
};
