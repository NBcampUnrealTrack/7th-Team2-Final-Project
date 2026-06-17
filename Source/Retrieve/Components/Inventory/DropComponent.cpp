#include "Components/Inventory/DropComponent.h"

#include "Engine/DataTable.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Components/Inventory/InventoryComponent.h"
#include "Components/Combat/RetrieveHealthComponent.h"
#include "Data/RetrieveDataTableTypes.h"

void UDropComponent::Initialize(UDataTable* InDropTable, const TArray<FName>& InDropRowNames)
{
	DropTable    = InDropTable;
	DropRowNames = InDropRowNames;
}

void UDropComponent::ProcessDrop()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		// 인벤토리 변경은 서버 권한에서만 수행
		return;
	}

	if (!DropTable || DropRowNames.Num() == 0)
	{
		UE_LOG(LogDataTable, Error, TEXT("[%s] DropTable not set."), *GetName());
		return;
	}

	// 마지막으로 데미지를 입힌 액터(플레이어) → 그 폰의 InventoryComponent를 찾는다.
	const URetrieveHealthComponent* HealthComp = OwnerActor->FindComponentByClass<URetrieveHealthComponent>();
	AActor* KillerActor = HealthComp ? HealthComp->GetLastDamageInstigator() : nullptr;

	APawn* KillerPawn = Cast<APawn>(KillerActor);
	if (!KillerPawn)
	{
		if (AController* KillerController = Cast<AController>(KillerActor))
		{
			KillerPawn = KillerController->GetPawn();
		}
	}

	UInventoryComponent* Inventory =
		KillerPawn ? KillerPawn->FindComponentByClass<UInventoryComponent>() : nullptr;
	if (!Inventory)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[%s] ProcessDrop: 마지막 공격자의 InventoryComponent를 찾지 못해 드랍을 건너뜁니다."),
			*OwnerActor->GetName());
		return;
	}

	// 각 드랍 행을 DropChance로 독립 굴림 → 성공 시 인벤토리에 지급
	for (const FName& RowName : DropRowNames)
	{
		if (RowName.IsNone())
		{
			continue;
		}

		const FEnemyDropRow* Row = DropTable->FindRow<FEnemyDropRow>(RowName, TEXT("UDropComponent::ProcessDrop"));
		if (!Row)
		{
			UE_LOG(LogDataTable, Error, TEXT("[%s] EnemyDropRow '%s' missing."), *GetName(), *RowName.ToString());
			continue;
		}

		if (Row->ItemId.IsNone())
		{
			continue;
		}

		const float Roll = FMath::FRand();
		if (Roll > Row->DropChance)
		{
			continue;
		}

		Inventory->AddItem(Row->ItemId, Row->ItemCategoryTag, Row->Quantity);

		UE_LOG(LogTemp, Log,
			TEXT("UDropComponent: %s → %s 인벤토리 지급 [Id=%s, Cat=%s, Qty=%d, Roll=%.2f/%.2f]"),
			*OwnerActor->GetName(),
			*KillerPawn->GetName(),
			*Row->ItemId.ToString(),
			*Row->ItemCategoryTag.ToString(),
			Row->Quantity,
			Roll,
			Row->DropChance);
	}
}
