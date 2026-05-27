
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"

#include "RetrieveModularMeshTypes.generated.h"

class USkeletalMesh;

/**
 * 특정 BoneName 파츠에 적용할 SkeletalMesh입니다.
 *
 * 이 시스템은 Actor 파츠/Socket Attach를 다루지 않습니다.
 * 같은 Skeleton을 공유하는 SkeletalMesh 모듈을 교체하고 LeaderPose를 따르게 하는 용도입니다.
 */
USTRUCT(BlueprintType)
struct FRetrieveModularPartMesh
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (GetOptions = "GetReferenceBoneNames"))
	FName BoneName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USkeletalMesh> Mesh = nullptr;
};

/**
 * 기준 스켈레탈 메시의 본 이름과 기본 모듈 메시를 그룹 단위로 정리합니다.
 * 예: HeadMeshes, BodyMeshes, HandsMeshes, FeetMeshes
 */
USTRUCT(BlueprintType)
struct FRetrieveModularPartGroup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName GroupName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (TitleProperty = BoneName))
	TArray<FRetrieveModularPartMesh> DefaultParts;
};

/**
 * 모듈식 스켈레탈 메시 캐릭터의 기준 Skeleton/BoneGroup 구조를 정의하는 데이터 에셋입니다.
 * 장비/코스메틱을 모두 제거했을 때 돌아갈 기본 파츠 메시도 함께 가집니다.
 */
UCLASS(BlueprintType)
class RETRIEVE_API URetrieveModularMeshLayout : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Modular Mesh")
	TObjectPtr<USkeletalMesh> ReferenceMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Modular Mesh", meta = (TitleProperty = GroupName))
	TArray<FRetrieveModularPartGroup> PartGroups;

	UFUNCTION()
	TArray<FName> GetReferenceBoneNames() const;
};

/**
 * 장비/코스메틱 한 덩어리가 어떤 BoneName 파츠의 Mesh를 교체하는지 정의합니다.
 * 예: 상체 방어구, 하체 방어구, 헤드 프리셋, 기본 바디 세트.
 */
UCLASS(BlueprintType)
class RETRIEVE_API URetrieveModularPartSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Modular Mesh")
	TObjectPtr<URetrieveModularMeshLayout> TargetLayout;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Modular Mesh")
	FName TargetGroup;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Modular Mesh", meta = (TitleProperty = BoneName))
	TArray<FRetrieveModularPartMesh> Parts;

	UFUNCTION()
	TArray<FName> GetReferenceBoneNames() const;
};

/**
 * 태그 조건을 만족했을 때 적용할 ModularMeshLayout입니다.
 * 예: Gender.Female → Layout_Female, Gender.Male → Layout_Male
 */
USTRUCT(BlueprintType)
struct FRetrieveModularLayoutSelectionEntry
{
	GENERATED_BODY()
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<URetrieveModularMeshLayout> Layout = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FGameplayTagContainer RequiredTags;
};

/**
 * 현재 태그 상태에 맞는 ModularMeshLayout을 선택합니다.
 * LayoutRules는 위에서 아래로 평가됩니다. 더 구체적인 조건을 앞에 배치하세요.
 */
USTRUCT(BlueprintType)
struct FRetrieveModularLayoutSelectionSet
{
	GENERATED_BODY()
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (TitleProperty = Layout))
	TArray<FRetrieveModularLayoutSelectionEntry> LayoutRules;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<URetrieveModularMeshLayout> DefaultLayout = nullptr;
	
	URetrieveModularMeshLayout* SelectBestLayout(const FGameplayTagContainer& Tags) const;
};