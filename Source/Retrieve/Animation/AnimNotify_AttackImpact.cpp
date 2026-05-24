#include "Animation/AnimNotify_AttackImpact.h"

#include "GameplayTags/RetrieveGameplayTags.h"
#include "Logging/RetrieveLogChannels.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"

UAnimNotify_AttackImpact::UAnimNotify_AttackImpact()
{
	EventTag = RetrieveGameplayTags::GameplayEvent_Attack_Impact;
}

FString UAnimNotify_AttackImpact::GetNotifyName_Implementation() const
{
	return TEXT("AttackImpact");
}

void UAnimNotify_AttackImpact::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!IsValid(MeshComp))
	{
		return;
	}

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!IsValid(OwnerActor))
	{
		UE_LOG(LogRetrieveCombat, Warning, TEXT("[AnimNotify_AttackImpact] Owner invalid. Mesh=%s"), *GetNameSafe(MeshComp));
		return;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwnerActor);
	if (!IsValid(ASC))
	{
		UE_LOG(LogRetrieveCombat, Warning, TEXT("[AnimNotify_AttackImpact] ASC missing. Owner=%s"), *GetNameSafe(OwnerActor));
		return;
	}

	const FGameplayTag EffectiveEventTag = EventTag.IsValid() ? EventTag : RetrieveGameplayTags::GameplayEvent_Attack_Impact;

	FGameplayEventData Payload;
	Payload.EventTag = EffectiveEventTag;
	Payload.Instigator = OwnerActor;
	
	// F3 피드백 반영: 이 시점은 트레이스 전 단계이므로 Target을 강제로 채우지 않고 nullptr 처리 (GA에서 타겟팅 결정)
	Payload.Target = nullptr; 
	Payload.EventMagnitude = ImpactMagnitude;
	
	// F2 피드백 반영: 공유 가능성이 높은 에셋 인스턴스(this) 포인터 전달 차단 및 제거
	Payload.OptionalObject = nullptr; 
	Payload.OptionalObject2 = Animation;

	// GAS 기반 전송 처리
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, EffectiveEventTag, Payload);

	UE_LOG(LogRetrieveCombat, Verbose,
		TEXT("[AnimNotify_AttackImpact] Owner=%s Event=%s Magnitude=%.2f"),
		*GetNameSafe(OwnerActor),
		*EffectiveEventTag.ToString(),
		ImpactMagnitude);
}
