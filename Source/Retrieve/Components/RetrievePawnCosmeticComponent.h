
#pragma once

#include "CoreMinimal.h"
#include "Components/PawnComponent.h"
#include "GameplayTagContainer.h"
#include "RetrievePawnCosmeticComponent.generated.h"


struct FGameplayEventData;
class URetrieveCosmeticData;
class UAbilitySystemComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RETRIEVE_API URetrievePawnCosmeticComponent : public UPawnComponent
{
	GENERATED_BODY()

public:
	URetrievePawnCosmeticComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	// SovereignCharacter::InitializeAbilitySystem()에서 호출
	void InitializeWithAbilitySystem(UAbilitySystemComponent* ASC);
	void UninitializeFromAbilitySystem();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|Cosmetic")
	TObjectPtr<URetrieveCosmeticData> CosmeticData;
	
private:
	UFUNCTION()
	void OnWeaponEquipped(FName WeaponItemId);

	UFUNCTION()
	void OnWeaponUnequipped(FName WeaponItemId);

	// GameplayEvent_Element_ModeChange 구독 콜백 — 원소 전환 완료 후 1회 호출
	void OnElementModeChanged(const FGameplayEventData* Payload);

	void ApplyCosmeticLayer();
	FGameplayTagContainer BuildCosmeticTags() const;

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> OwnerASC;

	FGameplayTag CurrentWeaponTypeTag;
	FGameplayTag CurrentElementTag;
	FGameplayTagContainer CosmeticTags;
};
