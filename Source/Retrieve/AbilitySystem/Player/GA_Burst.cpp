#include "AbilitySystem/Player/GA_Burst.h"

#include "GameplayTags/RetrieveGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Components/ElementGaugeComponent.h"
#include "Components/PlayerBurstComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameplayEffect.h"

UGA_Burst::UGA_Burst()
{
	AbilityTags.AddTag(RetrieveGameplayTags::Ability_Player_Burst);

	ActivationRequiredTags.AddTag(RetrieveGameplayTags::State_Gauge_Full);
}

void UGA_Burst::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
    UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (!IsValid(Avatar) || !IsValid(ASC) || !SkillCombinationTable)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
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

    TMap<FGameplayTag, int32> ElementPattern = Gauge->GetCurrentCombination();

    const FSkillCombination* MatchedRow = FindMatchingCombination(ElementPattern);
    if (!MatchedRow)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_Burst] No matching combination found"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    UE_LOG(LogTemp, Log, TEXT("[GA_Burst] Skill=%s"), *MatchedRow->DisplayName.ToString());

    Gauge->ConsumeAllSlots();

    UPlayerBurstComponent* BurstComp = Avatar->FindComponentByClass<UPlayerBurstComponent>();
    if (!IsValid(BurstComp))
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_Burst] PlayerBurstComponent not found on %s"), *Avatar->GetName());
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    UAnimMontage* Montage = MatchedRow->AttackMontage.LoadSynchronous();
    if (!IsValid(Montage))
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_Burst] AttackMontage is null for Skill=%s"), *MatchedRow->DisplayName.ToString());
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    CachedBurstComp = BurstComp;
    CachedSkill = MatchedRow;
    BurstComp->BeginBurstSkill(MatchedRow);

    MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage, 1.f, NAME_None, true);
    if (!MontageTask)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageCompleted);
    MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
    MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageCancelled);
    MontageTask->ReadyForActivation();
}

void UGA_Burst::HandleMontageCompleted()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Burst::HandleMontageInterrupted()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_Burst::HandleMontageCancelled()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_Burst::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (MontageTask)
    {
        MontageTask->EndTask();
        MontageTask = nullptr;
    }

    if (IsValid(CachedBurstComp))
    {
        CachedBurstComp->EndBurstSkill();
    }
    CachedBurstComp = nullptr;
    CachedSkill = nullptr;

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_Burst::CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility)
{
    if (MontageTask)
    {
        MontageTask->EndTask();
        MontageTask = nullptr;
    }

    if (IsValid(CachedBurstComp))
    {
        CachedBurstComp->EndBurstSkill();
    }
    CachedBurstComp = nullptr;
    CachedSkill = nullptr;

    Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
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
