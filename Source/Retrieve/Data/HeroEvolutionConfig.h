#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "HeroEvolutionConfig.generated.h"

/**
 * 잊혀진 영웅 → 전설 영웅 장비 진화 규칙 정의(데이터 에셋).
 * UHeroEquipmentEvolutionComponent가 이 설정으로 세트 완성 판정·충전·진화 스왑을 수행한다.
 *
 * 밸런싱은 전부 여기서: 세트 태그, 충전 임계치, 카운트 대상 어빌리티, 아이템 매핑.
 */
UCLASS(BlueprintType)
class RETRIEVE_API UHeroEvolutionConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ── 세트 완성 판정 ──────────────────────────────────────────────

	/** 잊혀진 영웅 무기(검+방패)의 WeaponSetTag. DT_Weapon 행의 WeaponSetTag와 매칭. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evolution|Set", meta = (Categories = "Weapon.Set"))
	FGameplayTag ForgottenWeaponSetTag;

	/** 잊혀진 영웅 방어구의 ArmorSetTag. DT_Armor 행의 ArmorSetTag와 매칭. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evolution|Set", meta = (Categories = "Armor.Set"))
	FGameplayTag ForgottenArmorSetTag;

	/** 세트로 인정할 최소 방어구 착용 부위 수(기본 4). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evolution|Set", meta = (ClampMin = "1"))
	int32 RequiredArmorPieceCount = 4;

	// ── 충전 ───────────────────────────────────────────────────────

	/** 진화에 필요한 누적 충전량(흡수/버스트 합산 횟수, 기본 30). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evolution|Charge", meta = (ClampMin = "1"))
	int32 ChargeThreshold = 30;

	/** 충전 1을 부여할 어빌리티 애셋 태그들(흡수/버스트). 활성화 시 이 중 하나라도 매칭되면 +1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evolution|Charge", meta = (Categories = "Ability"))
	FGameplayTagContainer QualifyingAbilityTags;

	// ── 진화 스왑 ───────────────────────────────────────────────────

	/** 잊혀진 아이템 ItemId → 전설 아이템 ItemId 매핑. 무기 1 + 방어구 4 = 5쌍. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evolution|Swap")
	TMap<FName, FName> ItemEvolutionMap;

	/** 무기 AddItem/RemoveItem에 쓰는 카테고리 태그(예: Item.Category.Weapon). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evolution|Swap", meta = (Categories = "Item"))
	FGameplayTag WeaponCategoryTag;

	/** 방어구 AddItem/RemoveItem에 쓰는 카테고리 태그(예: Item.Category.Armor). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evolution|Swap", meta = (Categories = "Item"))
	FGameplayTag ArmorCategoryTag;

	/** 지정된 잊혀진 ItemId의 전설 대응 ItemId를 반환. 없으면 NAME_None. */
	FName GetEvolvedItemId(FName ForgottenItemId) const
	{
		const FName* Found = ItemEvolutionMap.Find(ForgottenItemId);
		return Found ? *Found : NAME_None;
	}
};
