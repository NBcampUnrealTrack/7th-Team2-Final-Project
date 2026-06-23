
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/RetrieveDataTableTypes.h"
#include "AttackComboDefinition.generated.h"

/**
 * Anim Layer의 Element에 해당하는 Combo Attack Montage의 데이터를 관리하는 DataAsset
 */
UCLASS(BlueprintType)
class RETRIEVE_API UAttackComboDefinition : public UDataAsset
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
	
	// --- Parry Counter ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ParryCounter")
	FParryCounterData ParryDefault;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ParryCounter")
	TArray<FParryCounterData> ParryVariants;

	const FParryCounterData* ResolveParryVariant(const FGameplayTag& ElementTag) const;
};
