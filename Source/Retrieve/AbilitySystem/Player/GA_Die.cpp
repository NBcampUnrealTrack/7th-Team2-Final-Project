#include "AbilitySystem/Player/GA_Die.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/RetrieveAnimSlots.h"
#include "GameplayTags/RetrieveGameplayTags.h"

UGA_Die::UGA_Die()
{
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(RetrieveGameplayTags::Ability_Common_Die);
    SetAssetTags(AssetTags);
    
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
    
    ActivationBlockedTags.Reset();
}

void UGA_Die::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    if (!DeathMontage)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }
    
    MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this,
        NAME_None,
        DeathMontage,
        /*Rate=*/1.f,
        /*StartSection=*/NAME_None,
        /*bStopWhenAbilityEnds=*/true,
        /*AnimRootMotionTranslationScale=*/1.f,
        /*StartTimeSeconds=*/0.f);

    if (!MontageTask)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageCompleted);
    MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleMontageBlendOut);
    MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
    MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageCancelled);
    MontageTask->ReadyForActivation();
}

void UGA_Die::HandleMontageCompleted()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Die::HandleMontageBlendOut()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Die::HandleMontageInterrupted()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_Die::HandleMontageCancelled()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_Die::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled)
{
    if (MontageTask)
    {
        MontageTask->EndTask();
        MontageTask = nullptr;
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
