#include "Components/World/RetrieveShopComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "Shop/RetrieveShopDefinitionAsset.h"
#include "Shop/RetrieveShopTypes.h"
#include "Save/RetrieveSaveSubsystem.h"
#include "Save/RetrieveSaveGame.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Math/UnrealMathUtility.h"
#include "UObject/UnrealType.h"

namespace
{
	/** owner에서 이름이 "InteractionTarget"인 컴포넌트를 찾아 InteractionEnabled를 reflection으로 토글한다.
	 *  상점 UI가 열려있는 동안 이 NPC의 상호작용 프롬프트를 숨기고, 닫히면 복원하는 데 사용된다. */
	void SetShopInteractionTargetEnabled(AActor* Owner, bool bEnabled)
	{
		if (!Owner)
		{
			return;
		}

		TArray<UActorComponent*> Comps;
		Owner->GetComponents(Comps);
		for (UActorComponent* Comp : Comps)
		{
			if (Comp && Comp->GetFName() == TEXT("InteractionTarget"))
			{
				if (FBoolProperty* EnabledProp =
					FindFProperty<FBoolProperty>(Comp->GetClass(), TEXT("InteractionEnabled")))
				{
					EnabledProp->SetPropertyValue_InContainer(Comp, bEnabled);
				}
				break;
			}
		}
	}
}

URetrieveShopComponent::URetrieveShopComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void URetrieveShopComponent::BeginPlay()
{
	Super::BeginPlay();

	RollRotatingStock();

	if (UWorld* World = GetWorld())
	{
		RestListenerHandle = UGameplayMessageSubsystem::Get(World)
			.RegisterListener<FRetrievePlayerRestedPayload>(
				RetrieveGameplayTags::Channel_Player_Rested,
				[WeakThis = TWeakObjectPtr<URetrieveShopComponent>(this)]
				(FGameplayTag, const FRetrievePlayerRestedPayload&)
				{
					if (URetrieveShopComponent* Comp = WeakThis.Get())
						Comp->RollRotatingStock();
				});
	}
}

void URetrieveShopComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		UGameplayMessageSubsystem::Get(World).UnregisterListener(RestListenerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void URetrieveShopComponent::RollRotatingStock()
{
	CachedRotatingRows.Reset();

	if (!ShopDefinition || !ShopDefinition->RotatingPoolTable || ShopDefinition->RotatingSlotCount <= 0)
		return;

	// RotatingRowFilter가 있으면 그 목록만, 없으면 테이블 전체
	TArray<FName> Pool = ShopDefinition->RotatingRowFilter.Num() > 0
		? ShopDefinition->RotatingRowFilter
		: ShopDefinition->RotatingPoolTable->GetRowNames();

	// Fisher-Yates shuffle
	for (int32 i = Pool.Num() - 1; i > 0; --i)
	{
		const int32 j = FMath::RandRange(0, i);
		Pool.Swap(i, j);
	}

	const int32 Count = FMath::Min(ShopDefinition->RotatingSlotCount, Pool.Num());
	CachedRotatingRows.Append(Pool.GetData(), Count);
}

void URetrieveShopComponent::OpenShop(AActor* InstigatorActor)
{
	if (!ShopDefinition)
	{
		UE_LOG(LogTemp, Warning, TEXT("RetrieveShopComponent: ShopDefinition이 비어 있습니다."));
		return;
	}

	APlayerController* PC = nullptr;
	if (APawn* Pawn = Cast<APawn>(InstigatorActor))
	{
		PC = Cast<APlayerController>(Pawn->GetController());
	}

	if (!PC)
	{
		PC = InstigatorActor ? InstigatorActor->GetWorld()->GetFirstPlayerController() : nullptr;
	}

	// 유한 재고를 세이브에서 로드(세션 최초 1회). 패널이 재고를 조회하기 전에 준비한다.
	EnsureStockInitialized();

	// 상점 UI가 열려있는 동안 이 NPC의 상호작용 프롬프트를 숨긴다 (닫힐 때 PlayerController가 복원).
	SetShopInteractionTargetEnabled(GetOwner(), false);

	OnShopOpenRequested.Broadcast(ShopDefinition, PC);
}

// ────────────────────────────────────────────────────────────────────────────
// 재고 (유한 재고 상점, 세이브 지속)
// ────────────────────────────────────────────────────────────────────────────

FName URetrieveShopComponent::GetShopId() const
{
	return ShopDefinition ? ShopDefinition->GetFName() : NAME_None;
}

const FRetrieveShopItemRow* URetrieveShopComponent::FindDefinitionRow(FName RowName) const
{
	if (!ShopDefinition || RowName.IsNone())
	{
		return nullptr;
	}
	if (ShopDefinition->ShopItemTable)
	{
		if (const FRetrieveShopItemRow* Row =
			ShopDefinition->ShopItemTable->FindRow<FRetrieveShopItemRow>(RowName, TEXT("ShopStock")))
		{
			return Row;
		}
	}
	if (ShopDefinition->RotatingPoolTable)
	{
		if (const FRetrieveShopItemRow* Row =
			ShopDefinition->RotatingPoolTable->FindRow<FRetrieveShopItemRow>(RowName, TEXT("ShopStock")))
		{
			return Row;
		}
	}
	return nullptr;
}

void URetrieveShopComponent::EnsureStockInitialized()
{
	if (bStockInitialized)
	{
		return;
	}
	bStockInitialized = true;

	const FName ShopId = GetShopId();
	if (ShopId.IsNone())
	{
		return;
	}

	// 세이브에 저장된 소진 재고가 있으면 그대로 로드한다(구매로 줄어든 상태를 복원).
	if (UGameInstance* GI = UGameplayStatics::GetGameInstance(GetWorld()))
	{
		if (URetrieveSaveSubsystem* Sub = GI->GetSubsystem<URetrieveSaveSubsystem>())
		{
			if (URetrieveSaveGame* Save = Sub->GetCurrentSaveGame())
			{
				if (const FRetrieveShopStockSave* Saved = Save->ShopStock.Find(ShopId))
				{
					RemainingStockMap = Saved->RemainingStock;
				}
			}
		}
	}
}

int32 URetrieveShopComponent::GetRemainingStock(FName RowName) const
{
	// 이미 소진 기록이 있으면 그 값이 곧 남은 재고.
	if (const int32* Found = RemainingStockMap.Find(RowName))
	{
		return *Found;
	}

	// 기록이 없으면 정의의 StockCount가 시작 재고. 음수(-1)면 무한.
	const FRetrieveShopItemRow* Row = FindDefinitionRow(RowName);
	if (!Row || Row->StockCount < 0)
	{
		return -1;
	}
	return Row->StockCount;
}

bool URetrieveShopComponent::ConsumeStock(FName RowName, int32 Quantity)
{
	if (Quantity <= 0)
	{
		return true;
	}

	const int32 Remaining = GetRemainingStock(RowName);
	if (Remaining < 0)
	{
		// 무한 재고 — 추적하지 않는다.
		return true;
	}
	if (Remaining < Quantity)
	{
		return false;
	}

	RemainingStockMap.Add(RowName, Remaining - Quantity);
	PersistStock();
	return true;
}

void URetrieveShopComponent::PersistStock() const
{
	const FName ShopId = GetShopId();
	if (ShopId.IsNone())
	{
		return;
	}

	if (UGameInstance* GI = UGameplayStatics::GetGameInstance(GetWorld()))
	{
		if (URetrieveSaveSubsystem* Sub = GI->GetSubsystem<URetrieveSaveSubsystem>())
		{
			if (URetrieveSaveGame* Save = Sub->GetCurrentSaveGame())
			{
				FRetrieveShopStockSave& Entry = Save->ShopStock.FindOrAdd(ShopId);
				Entry.RemainingStock = RemainingStockMap;
				Sub->FlushWorldState();
			}
		}
	}
}
