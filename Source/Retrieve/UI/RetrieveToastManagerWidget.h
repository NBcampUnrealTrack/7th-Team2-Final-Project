#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "RetrieveToastManagerWidget.generated.h"

class UInventoryComponent;
class UTexture2D;
struct FRetrievePickupToastPayload;

/**
 * 아이템 획득 토스트 알림을 독립적으로 관리하는 위젯.
 *
 * ─ WBP_HUD와 완전히 분리된 독립 위젯이다 ─────────────────────────────────
 * WBP_HUD의 부모 클래스를 바꿀 필요 없이,
 * PlayerController가 이 위젯(WBP_ToastManager)을 별도로 Viewport에 추가한다.
 * HUD 담당자와 작업 범위가 충돌하지 않는다.
 *
 * ─ 동작 흐름 ─────────────────────────────────────────────────────────────
 * PlayerController → (InGame 상태 진입 시) CreateWidget + AddToViewport(ZOrder=60)
 * NativeConstruct  → 0.3초 후 InventoryComponent.OnItemAdded 바인딩
 *                    (BeginPlay 초기화 타이밍의 OnItemAdded 스팸 방지)
 * OnInventoryItemAdded → WBP_ItemPickupToast 생성 → ActiveToasts 큐 관리
 *                         → ToastLifetime초 후 자동 제거 → 재정렬
 *
 * ─ 블루프린트 설정 ────────────────────────────────────────────────────────
 * 1. Content Browser에서 이 클래스를 부모로 WBP_ToastManager 생성
 * 2. BP_RetrievePlayerController의 Details → ToastManagerClass 슬롯에 할당
 */
UCLASS()
class RETRIEVE_API URetrieveToastManagerWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	// ── 인벤토리 바인딩 ────────────────────────────────────────────
	UFUNCTION()
	void OnInventoryItemAdded(FName ItemId, FGameplayTag ItemCategoryTag, int32 Quantity);

	/** 골드 등 통화 변경 시 증가분만 토스트로 표시. */
	UFUNCTION()
	void OnCurrencyChanged(int32 NewAmount);

	/** NativeConstruct에서 TimerManager를 통해 지연 호출 */
	void BindToInventory();

	/** Channel.UI.PickupToast 메시지(퀘스트 물건 회수 등) 수신 → 커스텀 토스트. */
	void HandlePickupMessage(FGameplayTag Channel, const FRetrievePickupToastPayload& Payload);

	/** 이름/아이콘/수량 문구를 직접 지정해 토스트를 생성·큐잉한다. */
	void SpawnCustomToast(const FText& Title, UTexture2D* Icon, const FText& QuantityText);

	/** 생성된 토스트를 큐에 넣고 슬롯 배치 + 자동 제거 타이머를 건다(공통 처리). */
	void FinalizeToast(UUserWidget* Toast);

	UPROPERTY()
	TObjectPtr<UInventoryComponent> BoundInventoryComp;

	/** 0.3초 지연 바인딩용 타이머 핸들 */
	FTimerHandle BindDelayHandle;

	/** 통화 증가분 계산용 직전 값. 바인딩 시점에 현재 소지금으로 초기화(로드 스팸 방지). */
	int32 LastCurrency = 0;

	/** Channel.UI.PickupToast 리스너 핸들. */
	FGameplayMessageListenerHandle PickupListenerHandle;

	// ── 토스트 큐 ──────────────────────────────────────────────────
	/** 현재 화면에 표시 중인 토스트 위젯 목록 (오래된 순) */
	UPROPERTY()
	TArray<TObjectPtr<UUserWidget>> ActiveToasts;

	/** 토스트를 화면에서 제거하고 나머지를 재정렬한다 */
	void RemoveToast(UUserWidget* Toast);

	/** ActiveToasts 순서에 따라 각 토스트의 슬롯 인덱스를 갱신한다 */
	void RepositionToasts();

	// ── 토스트 표시 설정 ───────────────────────────────────────────
	/** 동시 표시 최대 개수 */
	static constexpr int32 MaxToasts     = 5;
	/** 토스트 자동 제거까지 대기 시간 (초) */
	static constexpr float ToastLifetime = 2.5f;
	/** center_right 앵커 기준 첫 번째 슬롯 Y 오프셋 (음수 = 화면 중앙 위쪽) */
	static constexpr float ToastStartY   = -140.f;
	/** 슬롯 하나의 높이 — 토스트 높이 + 간격 */
	static constexpr float ToastSlotH    = 72.f;
};
