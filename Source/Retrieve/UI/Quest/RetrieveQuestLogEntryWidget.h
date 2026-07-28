#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "UI/Quest/RetrieveQuestTypeStyleAsset.h"
#include "RetrieveQuestLogEntryWidget.generated.h"

/** 좌측 퀘스트 목록의 한 행 위젯(WBP_QuestLogEntry). LietView가 UQuestEntryViewModel을 주입합니다. */
UCLASS()
class RETRIEVE_API URetrieveQuestLogEntryWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

	/** Main/Side 퀘스트를 구분하는 외형 매핑. DA_QuestTypeStyle을 할당. */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Quest")
	TObjectPtr<URetrieveQuestTypeStyleAsset> StyleAsset;

	/** 해석된 스타일을 UMG 그래프에 전달 — 그래프가 프레임/아이콘 Image와 텍스트 색을 적용. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Retrieve|Quest")
	void ApplyQuestTypeStyle(const FRetrieveQuestTypeStyle& Style, bool bTracked, bool bCompleted);
};
