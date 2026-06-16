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
	void Initialize(UDataTable* InDropTable, FName InDropRowName);

	UFUNCTION(BlueprintCallable)
	void ProcessDrop();

private:
	UPROPERTY(EditDefaultsOnly, Category = "Drop")
	TObjectPtr<UDataTable> DropTable;

	UPROPERTY(EditDefaultsOnly, Category = "Drop")
	FName DropRowName;
};
