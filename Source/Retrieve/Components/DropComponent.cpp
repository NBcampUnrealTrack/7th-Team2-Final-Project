#include "Components/DropComponent.h"

#include "Engine/DataTable.h"
#include "StructUtils/InstancedStruct.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameplayTags/RetrieveGameplayTags.h"

void UDropComponent::Initialize(UDataTable* InDropTable, FName InDropRowName)
{
	DropTable    = InDropTable;
	DropRowName  = InDropRowName;
}

void UDropComponent::ProcessDrop()
{
	// Channel.Monster.Died 브로드캐스트 — 퀘스트·게임매니저가 구독
	if (UWorld* World = GetWorld())
	{
		UGameplayMessageSubsystem& MsgSubsys = UGameplayMessageSubsystem::Get(World);
		MsgSubsys.BroadcastMessage(RetrieveGameplayTags::Channel_Monster_Died, FInstancedStruct());
	}

	if (!DropTable || DropRowName.IsNone())
	{
		return;
	}

	const FEnemyDropRow* Row = DropTable->FindRow<FEnemyDropRow>(DropRowName, TEXT("UDropComponent"));
	if (!Row)
	{
		return;
	}

	const float Roll = FMath::FRand();
	if (Roll > Row->DropChance)
	{
		return;
	}

	// TODO (A3-c): 아이템 액터 스폰 시스템 완성 후 아래 로직 채워넣기
	// 현재는 드랍 데이터 확인용 로그만 출력
	UE_LOG(LogTemp, Log,
		TEXT("UDropComponent: %s 드랍 [Tag=%s, Qty=%d, Roll=%.2f/%.2f]"),
		*GetOwner()->GetName(),
		*Row->ItemTag.ToString(),
		Row->Quantity,
		Roll,
		Row->DropChance);
}
