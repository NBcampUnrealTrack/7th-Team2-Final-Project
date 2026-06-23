#include "AbilitySystem/Player/GA_Die.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/RetrieveAnimSlots.h"
#include "Character/RetrieveAlsCharacter.h"
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

    bRagdollTriggered = false;   // 부활 후 재사망 시 다시 ragdoll 가능하도록 리셋

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    if (!DeathMontage)
    {
        TriggerRagdoll();   // 몽타주 없어도 죽음은 ragdoll로 귀결
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
    TriggerRagdoll();
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Die::HandleMontageBlendOut()
{
    TriggerRagdoll();
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Die::HandleMontageInterrupted()
{
    TriggerRagdoll();
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_Die::HandleMontageCancelled()
{
    TriggerRagdoll();
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_Die::TriggerRagdoll()
{
    if (bRagdollTriggered)
    {
        return;
    }
    bRagdollTriggered = true;

    if (ARetrieveAlsCharacter* AlsCharacter = Cast<ARetrieveAlsCharacter>(GetAvatarActorFromActorInfo()))
    {
        AlsCharacter->StartRagdoll();
    }
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
