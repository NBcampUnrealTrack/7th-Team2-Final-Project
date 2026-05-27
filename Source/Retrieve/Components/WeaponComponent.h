#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveAbilitySet.h"
#include "Components/PawnComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameplayTagContainer.h"
#include "WeaponComponent.generated.h"

class URetrieveAbilitySystemComponent;
class UMeshComponent;
class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWeaponChangedSignature, FName, WeaponItemId);

// 장착된 무기의 전투 데이터, 비주얼, 무기 전용 어빌리티를 적용한다.
// 장착 가능 여부와 보유 검사는 InventoryComponent에서 먼저 처리한다.
UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent))
class RETRIEVE_API UWeaponComponent : public UPawnComponent
{
	GENERATED_BODY()

public:
	UWeaponComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Weapon")
	bool EquipWeapon(FName WeaponItemId);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Weapon")
	void UnequipWeapon();

	// 상태 조회: 장착 여부 → 어떤 무기 → 데이터 → 전투 테이블 순으로 배치
	UFUNCTION(BlueprintPure, Category = "Retrieve|Weapon")
	bool IsEquipped() const { return !CurrentWeaponDataRow.IsNone(); }

	UFUNCTION(BlueprintPure, Category = "Retrieve|Weapon")
	FName GetCurrentWeaponDataRow() const { return CurrentWeaponDataRow; }

	UFUNCTION(BlueprintPure, Category = "Retrieve|Weapon")
	FRetrieveWeaponDataRow GetWeaponData() const { return CurrentWeaponData; }

	UFUNCTION(BlueprintPure, Category = "Retrieve|Weapon")
	UDataTable* GetAttackTable() const { return CurrentWeaponAttackTable; }
	
	UFUNCTION(BlueprintPure, Category = "Retrieve|Weapon")
	UMeshComponent* GetPrimaryEquippedWeaponMesh() const;

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Weapon")
	FWeaponChangedSignature OnWeaponEquipped;

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Weapon")
	FWeaponChangedSignature OnWeaponUnequipped;

	const FRetrieveWeaponDataRow& GetWeaponDataRef() const { return CurrentWeaponData; }
	
protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Weapon")
	TObjectPtr<UDataTable> WeaponDataTable;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentWeaponDataRow, Category = "Retrieve|Weapon")
	FName CurrentWeaponDataRow = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Weapon")
	FGameplayTag CurrentWeaponTypeTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Weapon")
	FGameplayTag CurrentWeaponAffinityTag;

	UPROPERTY(Transient)
	FRetrieveWeaponDataRow CurrentWeaponData;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> CurrentWeaponAttackTable;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMeshComponent>> EquippedWeaponMeshComponents;

	UPROPERTY(Transient)
	FRetrieveAbilitySet_GrantedHandles WeaponGrantedHandles;

	UFUNCTION()
	void OnRep_CurrentWeaponDataRow();

	URetrieveAbilitySystemComponent* GetRetrieveAbilitySystemComponent() const;
	const FRetrieveWeaponDataRow* FindWeaponData(FName WeaponItemId) const;
	void ClearGrantedWeaponAbilities();
	void ClearWeaponVisuals();
	bool HasAuthorityToModify() const;
	bool ApplyWeaponData(FName WeaponItemId, const FRetrieveWeaponDataRow& WeaponData);
	bool ApplyWeaponVisuals(const FRetrieveWeaponDataRow& WeaponData);
	// 어태치먼트 데이터로 메시 컴포넌트를 생성해 반환. 메시 미설정 시 nullptr
	UMeshComponent* CreateWeaponMeshComponent(const FRetrieveWeaponAttachmentData& Attachment) const;
	USceneComponent* FindAttachmentParent(const FRetrieveWeaponAttachmentData& Attachment) const;
};
