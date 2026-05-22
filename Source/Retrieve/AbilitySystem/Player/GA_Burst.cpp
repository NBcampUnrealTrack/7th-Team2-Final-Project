#include "AbilitySystem/Player/GA_Burst.h"

#include "GameplayTags/RetrieveGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Components/ElementGaugeComponent.h"
#include "Data/RetrieveDataTableTypes.h"

UGA_Burst::UGA_Burst()
{
	AbilityTags.AddTag(RetrieveGameplayTags::Ability_Player_Burst);

	ActivationRequiredTags.AddTag(RetrieveGameplayTags::State_Gauge_Full);
}

void UGA_Burst::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    AActor* Avatar = ActorInfo->AvatarActor.Get();
    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
    if (!IsValid(Avatar) || !IsValid(ASC) || !SkillCombinationTable)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    UElementGaugeComponent* Gauge = Avatar->FindComponentByClass<UElementGaugeComponent>();
    if (!IsValid(Gauge))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    TMap<FGameplayTag, int32> ElementPattern;
    ElementPattern = Gauge->GetCurrentCombination();

    const FSkillCombination* MatchedRow = FindMatchingCombination(ElementPattern);
    if (!MatchedRow)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_Burst] No matching combination found"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    UE_LOG(LogTemp, Log, TEXT("[GA_Burst] Motion=%s"), *MatchedRow->MotionGroup.ToString());

    Gauge->ConsumeAllSlots();

    // (미구현) 실제 효과 발동 자리 ─ 추후:
    //    - PrimaryEffect 태그로 GameplayEvent 발행하여 sub-GA 트리거
    //    - MotionGroup으로 몽타주 선택
    //    - DamageMultiplier / AoeRadius로 GE 또는 Trace 파라미터화

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

const FSkillCombination* UGA_Burst::FindMatchingCombination(const TMap<FGameplayTag, int32>& ElementPattern) const
{
    if (!SkillCombinationTable) return nullptr;

    static const FString Context(TEXT("GA_Burst::FindMatchingCombination"));
    TArray<FSkillCombination*> Rows;
    SkillCombinationTable->GetAllRows<FSkillCombination>(Context, Rows);

    for (const FSkillCombination* Row : Rows)
    {
        if (Row && DoesCombinationMatch(Row->ElementPattern, ElementPattern))
        {
            return Row;
        }
    }
    return nullptr;
}

bool UGA_Burst::DoesCombinationMatch(const TMap<FGameplayTag, int32>& TablePattern, const TMap<FGameplayTag, int32>& CurrentPattern)
{
    if (TablePattern.Num() != CurrentPattern.Num()) return false;

    for (const TPair<FGameplayTag, int32>& Pair : TablePattern)
    {
        const int32* ElementCount = CurrentPattern.Find(Pair.Key);
        if (!ElementCount || *ElementCount != Pair.Value)
        {
            return false;
        }
    }
    return true;
}
