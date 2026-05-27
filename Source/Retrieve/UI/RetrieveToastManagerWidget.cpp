#include "UI/RetrieveToastManagerWidget.h"

#include "Components/InventoryComponent.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

// ─────────────────────────────────────────────────────────────────────────────
// 라이프사이클
// ─────────────────────────────────────────────────────────────────────────────

void URetrieveToastManagerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 초기화 타이밍의 OnItemAdded 스팸을 무시하기 위해 0.3초 후 바인딩
	// (InventoryComponent가 BeginPlay에서 기본 아이템을 채울 때 쏘는 브로드캐스트 방지)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			BindDelayHandle,
			this, &URetrieveToastManagerWidget::BindToInventory,
			0.3f, false);
	}
}

void URetrieveToastManagerWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindDelayHandle);
	}

	if (BoundInventoryComp)
	{
		BoundInventoryComp->OnItemAdded.RemoveDynamic(
			this, &URetrieveToastManagerWidget::OnInventoryItemAdded);
		BoundInventoryComp = nullptr;
	}

	// 남아있는 토스트 즉시 정리
	for (UUserWidget* Toast : ActiveToasts)
	{
		if (Toast) { Toast->RemoveFromParent(); }
	}
	ActiveToasts.Empty();

	Super::NativeDestruct();
}

// ─────────────────────────────────────────────────────────────────────────────
// 인벤토리 바인딩 (지연)
// ─────────────────────────────────────────────────────────────────────────────

void URetrieveToastManagerWidget::BindToInventory()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC) { return; }

	APawn* OwnerPawn = PC->GetPawn();
	if (!OwnerPawn) { return; }

	UInventoryComponent* InvComp =
		OwnerPawn->FindComponentByClass<UInventoryComponent>();
	if (!InvComp) { return; }

	InvComp->OnItemAdded.AddDynamic(
		this, &URetrieveToastManagerWidget::OnInventoryItemAdded);
	BoundInventoryComp = InvComp;
}

// ─────────────────────────────────────────────────────────────────────────────
// 아이템 획득 → 토스트 생성
// ─────────────────────────────────────────────────────────────────────────────

void URetrieveToastManagerWidget::OnInventoryItemAdded(
	FName ItemId, FGameplayTag ItemCategoryTag, int32 Quantity)
{
	// ── 토스트 클래스 로드 (최초 1회) ──────────────────────────────
	static TSubclassOf<UUserWidget> ToastClass = nullptr;
	if (!ToastClass)
	{
		ToastClass = LoadClass<UUserWidget>(
			nullptr,
			TEXT("/Game/Retrieve/UI/Interaction/WBP_ItemPickupToast.WBP_ItemPickupToast_C"));
	}

	if (!ToastClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[ToastManager] WBP_ItemPickupToast 클래스를 로드하지 못했습니다. "
			     "경로를 확인하세요: /Game/Retrieve/UI/Interaction/WBP_ItemPickupToast"));
		return;
	}

	// ── 최대 개수 초과 시 가장 오래된 토스트 제거 ──────────────────
	if (ActiveToasts.Num() >= MaxToasts)
	{
		RemoveToast(ActiveToasts[0]);
	}

	// ── 토스트 위젯 생성 ───────────────────────────────────────────
	UUserWidget* Toast = CreateWidget<UUserWidget>(this, ToastClass);
	if (!Toast) { return; }

	// InitToast(ItemId, ItemCategoryTag, Quantity) 호출
	{
		static const FName InitToastFuncName(TEXT("InitToast"));
		if (UFunction* Func = Toast->FindFunction(InitToastFuncName))
		{
			struct FInitToastParams
			{
				FName        ItemId;
				FGameplayTag ItemCategoryTag;
				int32        Quantity;
			};
			FInitToastParams Params{ ItemId, ItemCategoryTag, Quantity };
			Toast->ProcessEvent(Func, &Params);
		}
	}

	// SetToastSlotIndex(ActiveToasts.Num(), ToastStartY, ToastSlotH) 호출
	{
		static const FName SetSlotFuncName(TEXT("SetToastSlotIndex"));
		if (UFunction* Func = Toast->FindFunction(SetSlotFuncName))
		{
			struct FSetSlotParams
			{
				int32 SlotIndex;
				float SlotStartY;
				float SlotHeight;
			};
			FSetSlotParams Params{ ActiveToasts.Num(), ToastStartY, ToastSlotH };
			Toast->ProcessEvent(Func, &Params);
		}
	}

	ActiveToasts.Add(Toast);
	Toast->AddToViewport(10);

	// ── ToastLifetime초 후 자동 제거 ──────────────────────────────
	// TWeakObjectPtr 캡처: GC가 Toast를 회수해도 dangling pointer 방지
	TWeakObjectPtr<UUserWidget> WeakToast(Toast);
	FTimerHandle RemoveHandle;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RemoveHandle,
			FTimerDelegate::CreateWeakLambda(this, [this, WeakToast]()
			{
				if (UUserWidget* ToastPtr = WeakToast.Get())
				{
					RemoveToast(ToastPtr);
				}
			}),
			ToastLifetime, false);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// 토스트 제거 & 재정렬
// ─────────────────────────────────────────────────────────────────────────────

void URetrieveToastManagerWidget::RemoveToast(UUserWidget* Toast)
{
	// IsValid: GC 회수 여부 + nullptr 체크 동시 수행
	if (!IsValid(Toast)) { return; }

	// ActiveToasts에 없으면 이미 다른 경로로 제거된 것 (이중 제거 방지)
	if (!ActiveToasts.Contains(Toast)) { return; }

	Toast->RemoveFromParent();
	ActiveToasts.Remove(Toast);
	RepositionToasts();
}

void URetrieveToastManagerWidget::RepositionToasts()
{
	static const FName SetSlotFuncName(TEXT("SetToastSlotIndex"));

	// null/GC 항목 먼저 정리
	ActiveToasts.RemoveAll([](const TObjectPtr<UUserWidget>& W) { return !IsValid(W); });

	for (int32 i = 0; i < ActiveToasts.Num(); ++i)
	{
		UUserWidget* Toast = ActiveToasts[i];
		if (!IsValid(Toast)) { continue; }

		if (UFunction* Func = Toast->FindFunction(SetSlotFuncName))
		{
			struct FSetSlotParams
			{
				int32 SlotIndex;
				float SlotStartY;
				float SlotHeight;
			};
			FSetSlotParams Params{ i, ToastStartY, ToastSlotH };
			Toast->ProcessEvent(Func, &Params);
		}
	}
}
