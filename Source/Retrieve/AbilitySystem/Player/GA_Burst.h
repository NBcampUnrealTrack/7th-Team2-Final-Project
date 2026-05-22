#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "GA_Burst.generated.h"

struct FSkillCombination;

/**
 * 
 */
UCLASS()
class RETRIEVE_API UGA_Burst : public URetrieveGameplayAbility
{
	GENERATED_BODY()
	
public:
	UGA_Burst();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

private:
	// 테이블에서 매칭 조합 반환
	const FSkillCombination* FindMatchingCombination(const TMap<FGameplayTag, int32>& ElementPattern) const;
	// 조합 매칭 체크
	static bool DoesCombinationMatch(
		const TMap<FGameplayTag, int32>& TablePattern,
		const TMap<FGameplayTag, int32>& CurrentPattern);
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
	TObjectPtr<UDataTable> SkillCombinationTable;
};
