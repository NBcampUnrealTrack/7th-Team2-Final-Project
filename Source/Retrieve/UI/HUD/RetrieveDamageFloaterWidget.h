
#pragma once


#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RetrieveDamageFloaterWidget.generated.h"

class URetrieveDamageFloaterWidget;
class UTextBlock;

// 애니메이션이 끝난 플로터를 풀로 회수하기 위한 통지 델리게이트
DECLARE_DELEGATE_OneParam(FOnDamageFloaterFinished, URetrieveDamageFloaterWidget*);
/**
 * 대미지 숫자 플로터
 * 블루프린트 설정
 * 1. 이 클래스를 부모로 WBP_DamageFloater 생성
 * 2. 위젯 트리에 TextBlock을 이름 "DamageText"로 배치 (BindWidgetOptional 자동 바인딩)
 * 3. PlayFloaterAnim 이벤트에서 RiseFade 애니 재생
 * 4. 애니 종료에서 NotifyFinished 호출 → 풀 회수
 */
UCLASS()
class RETRIEVE_API URetrieveDamageFloaterWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 소유 컴포넌트가 호출 데이터 주입 후 WBP 애니메이션 시작 */
	void Activate(float DamageValue, float ScaleMultiplier, const FLinearColor& Color);
	/** 애니메이션이 끝난 플로터를 풀로 회수하기 위한 델리게이트. 소유 컴포넌트가 바인딩. */
	FOnDamageFloaterFinished OnFinished;

protected:
	/** WBP 위젯 트리의 "DamageText" TextBlock에 자동 바인딩 (없으면 nullptr). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DamageText;
	/** WBP 구현: RiseFade 상승·페이드 애니 재생. (데이터는 C++가 이미 세팅함) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Damage Floater")
	void PlayFloaterAnim();
	/** WBP가 애니메이션 종료 시 호출 → 풀 회수 통지. */
	UFUNCTION(BlueprintCallable, Category = "Damage Floater")
	void NotifyFinished();
};
