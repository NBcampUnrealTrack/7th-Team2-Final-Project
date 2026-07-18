#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RetrieveResonanceEntryWidget.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;

/** 공명 개요 목록 한 줄에 표시할 데이터 묶음(DT_ElementResonance + DT_BuffDefinitions 조합 결과). */
USTRUCT(BlueprintType)
struct FRetrieveResonanceEntryView
{
	GENERATED_BODY()

	/** 공명 이름 (DT_BuffDefinitions.DisplayName 우선, 없으면 DT_ElementResonance.DisplayName). */
	UPROPERTY(BlueprintReadOnly, Category = "Resonance")
	FText DisplayName;

	/** 필요 스택 표기 (예: "불2 + 물1"). */
	UPROPERTY(BlueprintReadOnly, Category = "Resonance")
	FText StacksText;

	/** 버프 효과 설명 (DT_BuffDefinitions.EffectSummary 우선). */
	UPROPERTY(BlueprintReadOnly, Category = "Resonance")
	FText EffectText;

	/** 버프 아이콘 (DT_BuffDefinitions.Icon). 없으면 null. */
	UPROPERTY(BlueprintReadOnly, Category = "Resonance")
	TObjectPtr<UTexture2D> Icon = nullptr;

	/** 아이콘 틴트 (DT_BuffDefinitions.TintColor). */
	UPROPERTY(BlueprintReadOnly, Category = "Resonance")
	FLinearColor IconTint = FLinearColor::White;
};

/**
 * RetrieveSkillOverviewWidget의 공명 목록 엔트리 위젯.
 * 아이콘 + 이름 + 필요 스택 + 버프 효과를 한 행으로 표시한다.
 *
 * WBP 선택 바인딩 이름(있는 것만 채움):
 * Image_Icon / Text_Name / Text_Stacks / Text_Effect
 */
UCLASS()
class RETRIEVE_API URetrieveResonanceEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 공명 한 줄 데이터를 위젯에 채운다. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Resonance")
	void SetEntry(const FRetrieveResonanceEntryView& InView);

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Resonance", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Icon;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Resonance", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Name;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Resonance", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Stacks;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Resonance", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Effect;
};
