#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "InventoryPreviewActor.generated.h"

class UAnimMontage;
class UArmorComponent;
class UDataTable;
class UInventoryComponent;
class USceneCaptureComponent2D;
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
	FName PreviewSceneCaptureComponentName = TEXT("SceneCaptureComp");

	// 설정 시 플레이어 ABP 대신 이 클래스를 프리뷰 리더 메시에 적용한다. ALS 비의존 ABP 사용 시 필수.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Inventory Preview")
	TSubclassOf<UAnimInstance> PreviewAnimClassOverride;

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
	void InitializePreviewAnimation();
	void MirrorPreviewLeaderMesh(USkeletalMeshComponent* Target, USkeletalMeshComponent* Source) const;
	void MirrorPreviewPartMesh(USkeletalMeshComponent* Target, USkeletalMeshComponent* Source, USkeletalMeshComponent* PoseSource) const;
	UDataTable* ResolveArmorDataTable() const;
	USkeletalMeshComponent* ResolvePreviewMeshComponent() const;
	USceneCaptureComponent2D* ResolveSceneCaptureComponent() const;
	void RefreshSceneCaptureShowOnlyList();
	const FRetrieveInventoryPreviewArmorMontage* FindArmorEquipMontage(FGameplayTag EquipmentSlotTag, FGameplayTag PartSlotTag) const;
	UAnimMontage* ResolveArmorSlotMontage(FGameplayTag EquipmentSlotTag) const;
	void BindInventoryEvents();
	void UnbindInventoryEvents();
};
