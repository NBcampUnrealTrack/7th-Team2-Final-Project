#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UI/VFX/RetrieveUIVFXWidget.h"
#include "RetrieveItemPickupToastWidget.generated.h"

class UImage;
class UTextBlock;
class UWidget;
class UWidgetAnimation;

/**
 * WBP_ItemPickupToast의 C++ 베이스 클래스.
 *
 * Blueprint의 InitToast 함수 내 GetDataTableRow 노드에 DataTable 핀이
 * 미설정되어 빈 텍스트가 표시되는 문제를 C++에서 직접 DataTable을 조회하여
 * 수정한다. BindWidgetOptional로 ItemNameText / QuantityText TextBlock을
 * 자동 바인딩하므로 bIsVariable 여부와 무관하게 동작한다.
 */
UCLASS()
class RETRIEVE_API URetrieveItemPickupToastWidget : public URetrieveUIVFXWidget
{
	GENERATED_BODY()

public:
	/**
	 * 아이템 이름과 수량 텍스트를 DataTable에서 조회하여 TextBlock에 설정한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|HUD")
	void InitToast(FName ItemId, FGameplayTag ItemCategoryTag, int32 Quantity);

	/**
	 * DataTable 조회 없이 직접 지정한 이름/아이콘/수량 문구로 토스트를 채운다.
	 * 골드 획득·퀘스트 물건 회수 등 인벤토리 아이템이 아닌 획득 피드백에 사용.
	 * @param Icon         표시할 아이콘(없으면 아이콘 숨김)
	 * @param QuantityText 수량 문구(예: "+100"). 비어 있으면 수량 숨김.
	 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|HUD")
	void InitCustomToast(const FText& Title, UTexture2D* Icon, const FText& InQuantityText);

	/**
	 * 스택 내 슬롯 인덱스를 설정한다.
	 * HUD에서 토스트를 추가/제거할 때마다 호출해 겹침 없이 수직으로 정렬한다.
	 * @param SlotIndex  0 = 가장 위 슬롯
	 * @param SlotStartY 첫 번째 슬롯의 화면 상단으로부터의 Y 오프셋 (픽셀)
	 * @param SlotHeight 슬롯 하나의 높이 (토스트 높이 + 간격)
	 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|HUD")
	void SetToastSlotIndex(int32 SlotIndex, float SlotStartY = 100.f, float SlotHeight = 90.f);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 이름을 WBP의 애니메이션 트랙(Anim_In/Anim_Out)과 동일하게 두면 UMG 블루프린트 컴파일러가
	// "Internal Compiler Error: Tried to create a property ... already exists" 로 컴파일에 실패한다.
	// (BindWidgetAnim 메타 없이도 UMG 컴파일러는 동일 이름의 애니메이션 프로퍼티를 매번 새로 생성 시도함)
	// 그래서 C++ 멤버 이름은 다르게 두고, FindAnimationByName 조회 문자열만 "Anim_In"/"Anim_Out"으로 유지한다.
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Retrieve|HUD|Animation")
	TObjectPtr<UWidgetAnimation> BoundAnim_In;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Retrieve|HUD|Animation")
	TObjectPtr<UWidgetAnimation> BoundAnim_Out;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|UI VFX|Toast")
	TObjectPtr<UWidget> ToastVFXTarget;

	// 위젯 트리의 ItemIconImage Image에 자동 바인딩 (없으면 nullptr)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> ItemIcon;

	// 위젯 트리의 ItemNameText TextBlock에 자동 바인딩 (없으면 nullptr)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ItemNameText;

	// 위젯 트리의 QuantityText TextBlock에 자동 바인딩 (없으면 nullptr)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QuantityText;

private:
	UWidgetAnimation* FindAnimationByName(FName AnimationName) const;

	void PlayExitAnimation();

	FTimerHandle ExitAnimationTimerHandle;

	/** ItemCategoryTag를 기반으로 적절한 DataTable에서 DisplayName을 조회한다. */
	static FText LookupItemDisplayName(FName ItemId, FGameplayTag ItemCategoryTag);

	/** 이름이 토스트 고정 폭을 넘으면 폰트 크기를 줄여 잘림을 방지한다(짧은 이름은 기본 크기 유지). */
	void ApplyNameAutoFit(const FText& Name);

	// 최초 호출 시 WBP에 설정된 기본 폰트 크기를 캐싱(토스트 재사용 시 원복 기준).
	int32 BaseNameFontSize = 0;
};
