#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "RetrieveModularMeshTypes.generated.h"

class USkeletalMesh;
class USkeletalMeshComponent;
class UMaterialInterface;
class URetrieveModularPartSet;

/**
 * 성별/체형처럼 기본 외형 축에 따라 VisualMesh에 적용할 통짜 바디 메시입니다.
 * 방어구 파츠는 이 메시를 LeaderPose로 따라갑니다.
 */
UCLASS(BlueprintType)
class RETRIEVE_API URetrieveCharacterVisualLayout : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<USkeletalMesh> BaseVisualMesh = nullptr;

	/** VisualMesh 아래에 항상 spawn되는 기본 바디 모듈러 파츠. 방어구는 이 위에 얹히고
	 *  필요 시 해당 PartSlotTag를 visibility suppression으로 가립니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<URetrieveModularPartSet> DefaultBodyPartSet = nullptr;

	/** 이 레이아웃으로 spawn되는 기본 바디 파츠의 모든 머티리얼 슬롯에 적용할 머티리얼.
	 *  비우면 각 메시 기본 머티리얼을 유지합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UMaterialInterface> BodyMaterialOverride = nullptr;
};

USTRUCT(BlueprintType)
struct FRetrieveVisualLayoutSelectionEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<URetrieveCharacterVisualLayout> Layout = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTagContainer RequiredTags;
};

USTRUCT(BlueprintType)
struct FRetrieveVisualLayoutSelectionSet
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (TitleProperty = Layout))
	TArray<FRetrieveVisualLayoutSelectionEntry> LayoutRules;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<URetrieveCharacterVisualLayout> DefaultLayout = nullptr;

	URetrieveCharacterVisualLayout* SelectBestLayout(const FGameplayTagContainer& Tags) const;
};

/**
 * VisualMesh와 같은 Skeleton을 공유하며 LeaderPose로 따라갈 방어구/코스메틱 파츠입니다.
 */
USTRUCT(BlueprintType)
struct FRetrieveSkinnedPartMesh
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName PartName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Cosmetic.Part"))
	FGameplayTag PartSlotTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<USkeletalMesh> Mesh = nullptr;
};

UCLASS(BlueprintType)
class RETRIEVE_API URetrieveModularPartSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Modular Mesh", meta = (TitleProperty = PartName))
	TArray<FRetrieveSkinnedPartMesh> SkinnedParts;

	/** 이 세트로 spawn되는 모든 파츠에 적용할 morph 값. 예: { "masculineFeminine": 1.0 }.
	 *  메시에 해당 morph가 없으면 무시됩니다. 활성 레이아웃의 바디 세트 morph는 방어구 파츠에도 적용됩니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Modular Mesh")
	TMap<FName, float> MorphTargets;
};

/**
 * 방어구/장비 하나의 파츠. DT row 안에 인라인으로 들어가며, soft 참조로 장착 시점에 로드합니다.
 * VisualMesh와 같은 Skeleton을 공유하고 LeaderPose로 따라갑니다.
 */
USTRUCT(BlueprintType)
struct FRetrieveArmorVisualPart
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "Cosmetic.Part"))
	FGameplayTag PartSlotTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<USkeletalMesh> Mesh;

	/** 이 파츠에 입힐 머티리얼. 비우면(Null) 메시 자체 머티리얼을 유지한다(폴백). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UMaterialInterface> MaterialOverride;

	/** MaterialOverride를 컴포넌트의 모든 머티리얼 슬롯에 적용한다. SetSkeletalMesh 이후에 호출할 것.
	 *  MaterialOverride가 Null이면 아무것도 하지 않는다(메시 기본 머티리얼 유지). */
	void ApplyMaterialOverride(USkeletalMeshComponent* Component) const;
};
