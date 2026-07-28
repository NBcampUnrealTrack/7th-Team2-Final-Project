
#pragma once

#include "CoreMinimal.h"
#include "Character/Cosmetics/SovereignAnimInstance.h"
#include "InventoryPreviewAnimInstance.generated.h"

class UAnimMontage;
class USkeletalMeshComponent;

/**
 * 인벤토리 프리뷰 전용 ABP 클래스 
 */
UCLASS()
class RETRIEVE_API UInventoryPreviewAnimInstance : public USovereignAnimInstance
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory Preview")
	void SetSourceMeshComponent(USkeletalMeshComponent* InSourceMeshComponent);
	
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory Preview")
	bool PlayPreviewActionMontage(UAnimMontage* Montage, float PlayRate = 1.0f);
	
	UPROPERTY(Transient, BlueprintReadWrite, Category = "Retrieve|Inventory Preview")
	TObjectPtr<USkeletalMeshComponent> SourceMeshComponent;
};
