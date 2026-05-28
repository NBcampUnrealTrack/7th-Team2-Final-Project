#include "Animation/AnimNotifyState_AttackImpact.h"

#include "GameplayTags/RetrieveGameplayTags.h"
#include "Logging/RetrieveLogChannels.h"
#include "AbilitySystemBlueprintLibrary.h"

UAnimNotifyState_AttackImpact::UAnimNotifyState_AttackImpact()
{
	EventTag = RetrieveGameplayTags::GameplayEvent_Attack_Impact;
}

FString UAnimNotifyState_AttackImpact::GetNotifyName_Implementation() const
{
	return TEXT("AttackImpactState");
}

void UAnimNotifyState_AttackImpact::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!IsValid(MeshComp)) return;
	AActor* OwnerActor = MeshComp->GetOwner();
	if (!IsValid(OwnerActor)) return;
	
	if (OwnerActor->GetLocalRole() != ROLE_Authority) return;
	
	FGameplayEventData BeginPayload;
	BeginPayload.EventTag = RetrieveGameplayTags::GameplayEvent_Attack_Impact_Begin;
	BeginPayload.Instigator = OwnerActor;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, BeginPayload.EventTag, BeginPayload);
}

void UAnimNotifyState_AttackImpact::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (!IsValid(MeshComp)) return;
	AActor* OwnerActor = MeshComp->GetOwner();
	if (!IsValid(OwnerActor)) return;
	
	if (OwnerActor->GetLocalRole() != ROLE_Authority) return;

	const FGameplayTag EffectiveEventTag = EventTag.IsValid() ? EventTag : RetrieveGameplayTags::GameplayEvent_Attack_Impact;
	
	FGameplayEventData Payload;
	Payload.EventTag = EffectiveEventTag;
	Payload.Instigator = OwnerActor;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, EffectiveEventTag, Payload);

	UE_LOG(LogRetrieveCombat, Verbose, TEXT("[AnimNotifyState_AttackImpact] Tick Owner=%s Event=%s dt=%.4f"), *GetNameSafe(OwnerActor), *EffectiveEventTag.ToString(), FrameDeltaTime);
}
