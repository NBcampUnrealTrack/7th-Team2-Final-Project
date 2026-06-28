
#pragma once

#include "CoreMinimal.h"
#include "RetrieveDataTableTypes.h"
#include "Engine/DataAsset.h"
#include "WeaponGuardAttackDefinition.generated.h"

/**
 * GuardAttack 전용 공격 데이터.
 * GuardAttack을 지원하는 무기만 UWeaponAttackDefinition에서 optional reference로 연결한다.
 *
 * 지원하지 않는 무기(Staff/Bow 등)는 UWeaponAttackDefinition::GuardAttackDefinition을 비워둔다.
 * 별도 bSupportsGuardAttack flag를 두지 않는 이유는 flag와 실제 data asset 참조가 서로 어긋나는 상황을 막기 위해서다.
 */
UCLASS()
class RETRIEVE_API UWeaponGuardAttackDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	// 원소별 variant가 없을 때 사용하는 기본 GuardAttack 데이터.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GuardAttack")
	FWeaponGuardAttackData Default;

	// 현재 원소 태그와 일치하는 항목이 있으면 Default보다 우선한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GuardAttack", meta = (TitleProperty = "ElementTag"))
	TArray<FWeaponGuardAttackData> Variants;

	const FWeaponGuardAttackData* ResolveGuardAttackVariant(const FGameplayTag& ElementTag) const;
};
