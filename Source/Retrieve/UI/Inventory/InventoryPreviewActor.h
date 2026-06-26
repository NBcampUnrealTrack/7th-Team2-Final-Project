#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "InventoryPreviewActor.generated.h"

class UAnimMontage;
class UArmorComponent;
class UDataTable;
class UInventoryComponent;
class USkeletalMeshComponent;

USTRUCT(BlueprintType)
struct FRetrieveInventoryPreviewArmorMontage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Inventory Preview", meta = (Categories = "Equipment.Slot"))
	FGameplayTag EquipmentSlotTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Inventory Preview", meta = (Categories = "Cosmetic.Part"))
	FGameplayTag PartSlotTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Inventory Preview")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Inventory Preview", meta = (ClampMin = "0.01"))
	float PlayRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Inventory Preview")
	FName StartSection = NAME_None;
};

UCLASS(Blueprintable)
class RETRIEVE_API AInventoryPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	AInventoryPreviewActor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory Preview")
	void ClearArmorMeshes();

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory Preview")
	void UpdateArmorPreview();

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory Preview|Animation")
	bool PlayArmorEquipMontage(FGameplayTag EquipmentSlotTag, FGameplayTag PartSlotTag);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Inventory Preview")
	FName PreviewSkeletalMeshComponentName = TEXT("SkeletalMeshComp");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Inventory Preview")
	TObjectPtr<UDataTable> ArmorDataTableOverride;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Inventory Preview|Animation", meta = (TitleProperty = EquipmentSlotTag))
	TArray<FRetrieveInventoryPreviewArmorMontage> ArmorEquipMontages;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Inventory Preview|Animation|Armor", meta = (DisplayName = "Montage Armor Head"))
	TObjectPtr<UAnimMontage> Montage_ArmorHead = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Inventory Preview|Animation|Armor", meta = (DisplayName = "Montage Armor Chest"))
	TObjectPtr<UAnimMontage> Montage_ArmorChest = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Inventory Preview|Animation|Armor", meta = (DisplayName = "Montage Armor Hands"))
	TObjectPtr<UAnimMontage> Montage_ArmorHands = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Inventory Preview|Animation|Armor", meta = (DisplayName = "Montage Armor Legs"))
	TObjectPtr<UAnimMontage> Montage_ArmorLegs = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Inventory Preview|Animation|Armor", meta = (DisplayName = "Montage Armor Feet"))
	TObjectPtr<UAnimMontage> Montage_ArmorFeet = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Inventory Preview|Animation|Armor", meta = (DisplayName = "Montage Armor Default"))
	TObjectPtr<UAnimMontage> Montage_ArmorDefault = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Inventory Preview|Animation|Armor", meta = (ClampMin = "0.01"))
	float ArmorEquipMontagePlayRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Inventory Preview|Animation|Armor")
	FName ArmorEquipMontageStartSection = NAME_None;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Retrieve|Inventory Preview")
	TArray<TObjectPtr<USkeletalMeshComponent>> ArmorMeshComponents;

	UPROPERTY(Transient)
	TObjectPtr<UInventoryComponent> BoundInventoryComponent;

	UPROPERTY(Transient)
	TObjectPtr<UArmorComponent> BoundArmorComponent;

	UFUNCTION()
	void HandleEquippedArmorChanged(FGameplayTag EquipmentSlotTag, FName ArmorItemId);

	UInventoryComponent* ResolveInventoryComponent() const;
	UArmorComponent* ResolveArmorComponent() const;
	USkeletalMeshComponent* ResolvePlayerBodyMeshComponent() const;
	// Source 컴포넌트의 메시/머티리얼/가시성을 Target에 복제하고, 포즈는 PoseSource를 LeaderPose로 따른다.
	void MirrorPlayerMeshComponent(USkeletalMeshComponent* Target, USkeletalMeshComponent* Source, USkeletalMeshComponent* PoseSource) const;
	UDataTable* ResolveArmorDataTable() const;
	USkeletalMeshComponent* ResolvePreviewMeshComponent() const;
	const FRetrieveInventoryPreviewArmorMontage* FindArmorEquipMontage(FGameplayTag EquipmentSlotTag, FGameplayTag PartSlotTag) const;
	UAnimMontage* ResolveArmorSlotMontage(FGameplayTag EquipmentSlotTag) const;
	void BindInventoryEvents();
	void UnbindInventoryEvents();
};
