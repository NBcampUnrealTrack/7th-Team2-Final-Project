#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveAbilitySet.h"
#include "ActiveGameplayEffectHandle.h"
#include "Components/PawnComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "ArmorComponent.generated.h"

class UGameplayEffect;
class URetrieveAbilitySystemComponent;
class URetrievePawnCosmeticComponent;
class UDataTable;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FArmorChangedSignature, FGameplayTag, EquipmentSlotTag, FName, ArmorItemId);

// 장착된 방어구 RowName만 복제하고, 각 클라이언트가 DataTable/DA를 읽어 외형을 재구성한다.
UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent))
class RETRIEVE_API UArmorComponent : public UPawnComponent
{
	GENERATED_BODY()

public:
	UArmorComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Armor")
	bool EquipArmor(FGameplayTag EquipmentSlotTag, FName ArmorItemId);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Armor")
	bool UnequipArmor(FGameplayTag EquipmentSlotTag);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Armor")
	void UnequipAllArmor();

	UFUNCTION(BlueprintPure, Category = "Retrieve|Armor")
	FName GetEquippedArmorId(FGameplayTag EquipmentSlotTag) const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Armor")
	TArray<FRetrieveEquippedArmorEntry> GetEquippedArmorEntries() const { return EquippedArmorEntries; }

	UFUNCTION(BlueprintPure, Category = "Retrieve|Armor")
	UDataTable* GetArmorDataTable() const { return ArmorDataTable; }

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Armor")
	FArmorChangedSignature OnArmorEquipped;

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Armor")
	FArmorChangedSignature OnArmorUnequipped;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Armor")
	TObjectPtr<UDataTable> ArmorDataTable;

	// 방어구 장착 시 캐릭터 Defense에 가산할 GameplayEffect
	// GE 설정: Duration=Infinite, Modifier=Defense Add, Magnitude=SetByCaller(Data.Armor.Defense)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Armor|Stats")
	TSubclassOf<UGameplayEffect> ArmorDefenseEffect;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_EquippedArmorEntries, Category = "Retrieve|Armor")
	TArray<FRetrieveEquippedArmorEntry> EquippedArmorEntries;

	UFUNCTION()
	void OnRep_EquippedArmorEntries();

private:
	const FRetrieveArmorDataRow* FindArmorData(FName ArmorItemId) const;
	const FRetrieveEquippedArmorEntry* FindEquippedArmorEntry(FGameplayTag EquipmentSlotTag) const;
	FRetrieveEquippedArmorEntry* FindMutableEquippedArmorEntry(FGameplayTag EquipmentSlotTag);
	URetrieveAbilitySystemComponent* GetRetrieveAbilitySystemComponent() const;
	URetrievePawnCosmeticComponent* GetPawnCosmeticComponent() const;

	bool HasAuthorityToModify() const;
	void ClearArmorGameplayForSlot(FGameplayTag EquipmentSlotTag);
	void ClearAllArmorGameplay();
	void ApplyArmorGameplay(FGameplayTag EquipmentSlotTag, const FRetrieveArmorDataRow& ArmorData);
	bool ApplyArmorVisual(FGameplayTag EquipmentSlotTag, const FRetrieveArmorDataRow& ArmorData);
	void RefreshArmorVisuals();

	UPROPERTY(Transient)
	TMap<FGameplayTag, FRetrieveAbilitySet_GrantedHandles> ArmorGrantedHandlesBySlot;

	UPROPERTY(Transient)
	TMap<FGameplayTag, FActiveGameplayEffectHandle> ArmorDefenseEffectHandlesBySlot;
};
