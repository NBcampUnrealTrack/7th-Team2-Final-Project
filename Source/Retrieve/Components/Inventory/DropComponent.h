#pragma once

#include "CoreMinimal.h"
#include "Components/PawnComponent.h"
#include "DropComponent.generated.h"

class UDataTable;

UCLASS(ClassGroup = "Retrieve", meta = (BlueprintSpawnableComponent))
class RETRIEVE_API UDropComponent : public UPawnComponent
{
	GENERATED_BODY()

public:
	void Initialize(UDataTable* InDropTable, const TArray<FName>& InDropRowNames);

	/** 사망 시 호출. DropRows의 각 행을 독립 굴림해 마지막 공격 플레이어의 인벤토리로 직접 지급한다. (서버 전용) */
	UFUNCTION(BlueprintCallable)
	void ProcessDrop();

private:
	UPROPERTY(EditDefaultsOnly, Category = "Drop")
	TObjectPtr<UDataTable> DropTable;

	UPROPERTY(EditDefaultsOnly, Category = "Drop")
	TArray<FName> DropRowNames;
};
