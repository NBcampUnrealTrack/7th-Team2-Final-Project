
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/RetrieveDataTableTypes.h"
#include "WeaponAttackDefinition.generated.h"

class UWeaponGuardAttackDefinition;

/**
 * 무기의 공격 데이터(콤보/Sprint/Bash/Jump/ParryCounter/Heavy/패리성공)를 담는 DataAsset
 */
UCLASS(BlueprintType)
class RETRIEVE_API UWeaponAttackDefinition : public UDataAsset
{
	GENERATED_BODY()
	
public:
	// --- Combo Attack ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ComboAttack")
	FAttackComboVariant DefaultVariant;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ComboAttack")
	TArray<FAttackComboVariant> ElementVariants;
	
	const FAttackComboVariant* ResolveComboVariant(const FGameplayTag& ElementTag) const;
	
	// --- Sprint Attack (Dash) ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SprintAttack")
	FWeaponSprintAttack SprintDefault;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SprintAttack")
	TArray<FWeaponSprintAttack> SprintVariants;
	
	const FWeaponSprintAttack* ResolveSprintVariant(const FGameplayTag& ElementTag) const;

	// --- Shield Bash (Sprint Attack의 제자리/캔슬 변형. 구조 동일 → FWeaponSprintAttack 재사용) ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShieldBash")
	FWeaponSprintAttack BashDefault;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShieldBash")
	TArray<FWeaponSprintAttack> BashVariants;

	const FWeaponSprintAttack* ResolveBashVariant(const FGameplayTag& ElementTag) const;

	// --- Jump Attack (Leap) ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JumpAttack")
	FWeaponJumpAttack JumpDefault;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JumpAttack")
	TArray<FWeaponJumpAttack> JumpVariants;

	const FWeaponJumpAttack* ResolveJumpVariant(const FGameplayTag& ElementTag) const;

	// --- Absorb Cast ---
	// GA_Absorb는 공용 AbilitySet에서 부여되지만, 시전 몽타주는 장착 무기별 AttackDefinition에서 해결한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Absorb")
	FWeaponAbsorbCast AbsorbDefault;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Absorb")
	TArray<FWeaponAbsorbCast> AbsorbVariants;

	const FWeaponAbsorbCast* ResolveAbsorbVariant(const FGameplayTag& ElementTag) const;
	
	// --- Parry Counter ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ParryCounter")
	FParryCounterData ParryDefault;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ParryCounter")
	TArray<FParryCounterData> ParryVariants;

	const FParryCounterData* ResolveParryVariant(const FGameplayTag& ElementTag) const;

	// --- Guard Attack (optional) ---
	// Parry 계열 데이터 아래에 둔다. GuardAttack은 "방패 공격"이면서 동시에 "패링 시도 액션"이기 때문이다.
	// Staff/Bow처럼 GuardAttack을 지원하지 않는 무기는 이 참조를 비워둔다(null = 미지원).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GuardAttack")
	TSoftObjectPtr<UWeaponGuardAttackDefinition> GuardAttackDefinition;

	// --- Heavy Attack (강공격) ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HeavyAttack")
	TArray<FWeaponHeavyAttack> HeavyVariants;

	const FWeaponHeavyAttack* ResolveHeavyVariant(const FGameplayTag& ElementTag) const;

	// --- Parry Success Reaction ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ParrySuccess")
	TSoftObjectPtr<UAnimMontage> ParrySuccessMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ParrySuccess", meta = (ClampMin = "0.1"))
	float ParrySuccessMontagePlayRate = 1.0f;
};
