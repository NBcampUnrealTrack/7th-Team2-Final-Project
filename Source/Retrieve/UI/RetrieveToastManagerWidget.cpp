#include "UI/RetrieveToastManagerWidget.h"

#include "Components/Inventory/InventoryComponent.h"
#include "Settings/RetrieveGameUserSettings.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Messaging/GameplayMessages/RetrieveGameplayMessageTypes.h"
#include "UI/RetrieveItemPickupToastWidget.h"

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
		BoundInventoryComp->OnCurrencyChanged.RemoveDynamic(
			this, &URetrieveToastManagerWidget::OnCurrencyChanged);
		BoundInventoryComp = nullptr;
	}

	if (PickupListenerHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			UGameplayMessageSubsystem::Get(World).UnregisterListener(PickupListenerHandle);
		}
		PickupListenerHandle = FGameplayMessageListenerHandle();
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

	// 통화(골드) 증가 토스트: 현재 소지금을 기준값으로 잡아 로드/초기화 스팸을 방지한다.
	LastCurrency = InvComp->GetCurrency();
	InvComp->OnCurrencyChanged.AddDynamic(
		this, &URetrieveToastManagerWidget::OnCurrencyChanged);

	BoundInventoryComp = InvComp;

	// 인벤토리 미경유 획득(퀘스트 물건 등) 토스트 메시지 구독.
	if (UWorld* World = GetWorld())
	{
		PickupListenerHandle = UGameplayMessageSubsystem::Get(World).RegisterListener<FRetrievePickupToastPayload>(
			RetrieveGameplayTags::Channel_UI_PickupToast,
			this, &URetrieveToastManagerWidget::HandlePickupMessage);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// 아이템 획득 → 토스트 생성
// ─────────────────────────────────────────────────────────────────────────────

void URetrieveToastManagerWidget::OnInventoryItemAdded(
	FName ItemId, FGameplayTag ItemCategoryTag, int32 Quantity)
{
	// "HUD 숨기기" 설정이 켜져 있으면 토스트는 별도 최상위 위젯이라 매니저 숨김과 무관하게
	// 새로 뜨므로, 생성 자체를 막는다(상시 숨김 게이팅).
	if (const URetrieveGameUserSettings* Settings = URetrieveGameUserSettings::Get();
		Settings && Settings->bHideHUD)
	{
		return;
	}

	// ── 최신 토스트 GeneratedClass 조회 ────────────────────────────
	// 함수 로컬 static UClass 포인터는 WBP 재컴파일 시 새 GeneratedClass로
	// 교체되지 않아, 런타임에서 애니메이션이 없는 이전 클래스를 생성할 수 있다.
	// LoadClass는 이미 로드된 최신 클래스를 빠르게 반환하므로 매번 다시 조회한다.
	const TSubclassOf<UUserWidget> ToastClass = LoadClass<UUserWidget>(
		nullptr,
		TEXT("/Game/Retrieve/UI/Interaction/WBP_ItemPickupToast.WBP_ItemPickupToast_C"));

	if (!ToastClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[ToastManager] WBP_ItemPickupToast 클래스를 로드하지 못했습니다. "
			     "경로를 확인하세요: /Game/Retrieve/UI/Interaction/WBP_ItemPickupToast"));
		return;
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

	FinalizeToast(Toast);
}

// ─────────────────────────────────────────────────────────────────────────────
// 골드(통화) 증가 → 토스트
// ─────────────────────────────────────────────────────────────────────────────

void URetrieveToastManagerWidget::OnCurrencyChanged(int32 NewAmount)
{
	const int32 Delta = NewAmount - LastCurrency;
	LastCurrency = NewAmount;

	// 증가분만 알린다(소비/차감은 토스트 없음).
	if (Delta <= 0)
	{
		return;
	}

	UTexture2D* GoldIcon = LoadObject<UTexture2D>(
		nullptr,
		TEXT("/Game/External/UIFantasyWarriorHUD/Textures/Icons_Inventory/"
		     "T_ICON_FantasyWarrior_Inventory_Currency01_Clean."
		     "T_ICON_FantasyWarrior_Inventory_Currency01_Clean"));

	SpawnCustomToast(
		NSLOCTEXT("Retrieve", "GoldToastTitle", "골드"),
		GoldIcon,
		FText::FromString(FString::Printf(TEXT("+%d"), Delta)));
}

// ─────────────────────────────────────────────────────────────────────────────
// 퀘스트 물건 회수 등 메시지 → 토스트
// ─────────────────────────────────────────────────────────────────────────────

void URetrieveToastManagerWidget::HandlePickupMessage(
	FGameplayTag Channel, const FRetrievePickupToastPayload& Payload)
{
	SpawnCustomToast(Payload.Title, Payload.Icon, Payload.QuantityText);
}

// ─────────────────────────────────────────────────────────────────────────────
// 커스텀 토스트 생성(골드/퀘스트 물건 공용)
// ─────────────────────────────────────────────────────────────────────────────

void URetrieveToastManagerWidget::SpawnCustomToast(
	const FText& Title, UTexture2D* Icon, const FText& QuantityText)
{
	if (const URetrieveGameUserSettings* Settings = URetrieveGameUserSettings::Get();
		Settings && Settings->bHideHUD)
	{
		return;
	}

	const TSubclassOf<UUserWidget> ToastClass = LoadClass<UUserWidget>(
		nullptr,
		TEXT("/Game/Retrieve/UI/Interaction/WBP_ItemPickupToast.WBP_ItemPickupToast_C"));
	if (!ToastClass) { return; }

	UUserWidget* Toast = CreateWidget<UUserWidget>(this, ToastClass);
	if (!Toast) { return; }

	if (URetrieveItemPickupToastWidget* ToastWidget = Cast<URetrieveItemPickupToastWidget>(Toast))
	{
		ToastWidget->InitCustomToast(Title, Icon, QuantityText);
	}

	FinalizeToast(Toast);
}

// ─────────────────────────────────────────────────────────────────────────────
// 토스트 큐잉·배치·수명(공통)
// ─────────────────────────────────────────────────────────────────────────────

void URetrieveToastManagerWidget::FinalizeToast(UUserWidget* Toast)
{
	if (!IsValid(Toast)) { return; }

	// ── 최대 개수 초과 시 가장 오래된 토스트 제거 ──────────────────
	if (ActiveToasts.Num() >= MaxToasts)
	{
		RemoveToast(ActiveToasts[0]);
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
	Toast->AddToViewport(60);

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
