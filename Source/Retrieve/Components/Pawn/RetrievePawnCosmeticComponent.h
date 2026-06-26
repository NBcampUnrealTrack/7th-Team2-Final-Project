
#pragma once

#include "CoreMinimal.h"
#include "Components/PawnComponent.h"
#include "GameplayTagContainer.h"
#include "Character/Cosmetics/RetrieveModularMeshTypes.h"
#include "RetrievePawnCosmeticComponent.generated.h"


struct FGameplayEventData;
class URetrieveCharacterVisualLayout;
class URetrieveCosmeticData;
class URetrieveModularPartSet;
class UAbilitySystemComponent;
class UMaterialInterface;
class USkeletalMesh;
class USkeletalMeshComponent;

USTRUCT()
struct FRetrieveSpawnedEquipmentVisuals
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TArray<TObjectPtr<USkeletalMeshComponent>> Components;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RETRIEVE_API URetrievePawnCosmeticComponent : public UPawnComponent
{
	GENERATED_BODY()

public:
	URetrievePawnCosmeticComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	// SovereignCharacter::InitializeAbilitySystem()에서 호출
	void InitializeWithAbilitySystem(UAbilitySystemComponent* ASC);
	void UninitializeFromAbilitySystem();
	// 방어구 DT row의 인라인 파츠/억제 데이터를 받아 해당 슬롯의 장비 visual을 재구성한다.
	void ApplyEquipmentPartsForSlot(FGameplayTag EquipmentSlotTag, const TArray<FRetrieveArmorVisualPart>& VisualParts, const FGameplayTagContainer& SuppressedDefaultPartSlots);
	void ClearEquipmentVisualSlot(FGameplayTag EquipmentSlotTag);
	void ClearAllEquipmentVisualSlots();
	void RefreshCosmeticState();

	const TMap<FName, float>& GetCurrentMorphTargets() const { return CurrentMorphTargets; }

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

	// 무기 타입은 단일값 축이다. Unarmed/이전 무기 태그를 제거하고 새 태그로 교체한다.
	void SetWeaponTypeTag(FGameplayTag NewWeaponTypeTag);

	void ApplyCosmeticLayer();
	void ApplyVisualLayout();

	// 기본 바디 파츠 (레이아웃당 1회 생성, 장비 변화로 destroy하지 않고 visibility로만 제어)
	void ApplyDefaultBodyPartSet(const URetrieveModularPartSet* PartSet);
	void ClearDefaultBodyParts();
	void RefreshDefaultPartVisibility();

	// 장비 visual 슬롯
	void RemoveSpawnedEquipmentForSlot(FGameplayTag EquipmentSlotTag);
	void ApplyEquipmentSuppressionForSlot(FGameplayTag EquipmentSlotTag, const TArray<FRetrieveArmorVisualPart>& VisualParts, const FGameplayTagContainer& ExplicitSuppressed);
	void ClearEquipmentSuppressionForSlot(FGameplayTag EquipmentSlotTag);

	USkeletalMeshComponent* CreateModularPartComponent(USkeletalMesh* Mesh, USkeletalMeshComponent* LeaderMesh, AActor* Owner, bool bCastShadow);
	// CurrentMorphTargets(활성 레이아웃의 바디 세트 morph)를 spawn 파츠에 적용. 메시에 없는 morph는 무시된다.
	void ApplyMorphTargets(USkeletalMeshComponent* MeshComponent) const;
	// 활성 레이아웃의 BodyMaterialOverride를 파츠의 모든 머티리얼 슬롯에 적용. 비어있으면 무시.
	void ApplyBodyMaterial(USkeletalMeshComponent* MeshComponent) const;
	void ApplyMorphTargetsToSpawnedParts() const;
	void ClearSpawnedModularParts();
	USkeletalMeshComponent* GetLeaderMeshComponent() const;
	FGameplayTagContainer BuildCosmeticTags() const;

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> OwnerASC;

	UPROPERTY(Transient)
	TObjectPtr<URetrieveCharacterVisualLayout> CurrentVisualLayout;

	// PartSlotTag -> 기본 바디 파츠 컴포넌트 (슬롯당 1개)
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<USkeletalMeshComponent>> SpawnedDefaultBodyParts;

	// EquipmentSlotTag -> 그 슬롯이 spawn한 장비 파츠들
	UPROPERTY(Transient)
	TMap<FGameplayTag, FRetrieveSpawnedEquipmentVisuals> SpawnedEquipmentVisuals;

	// EquipmentSlotTag -> 그 장비가 가리는 기본 PartSlot 집합 (자동 + 명시 합집합)
	TMap<FGameplayTag, FGameplayTagContainer> SuppressedDefaultPartSlotsByEquipmentSlot;

	// 활성 레이아웃의 바디 PartSet에서 가져온 morph 프로파일. 바디 + 방어구 파츠에 일괄 적용한다.
	TMap<FName, float> CurrentMorphTargets;

	// 활성 레이아웃의 BodyMaterialOverride. 기본 바디 파츠 spawn 시 적용한다.
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> CurrentBodyMaterial;

	FGameplayTag CurrentWeaponTypeTag;
	FGameplayTag CurrentElementTag;
	FGameplayTagContainer CosmeticTags;
};
