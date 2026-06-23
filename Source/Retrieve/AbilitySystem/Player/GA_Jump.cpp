#include "AbilitySystem/Player/GA_Jump.h"

#include "GameFramework/Character.h"
#include "GameplayTags/RetrieveGameplayTags.h"

UGA_Jump::UGA_Jump()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_Jump);
	SetAssetTags(Tags);

	// 사망/경직/다운 중에는 점프 불가.
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dead);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Knockdown);

	// 공격 중에는 점프 잠금. 공격 캔슬은 구르기/스킬로 처리(점프로 캔슬하지 않음).
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Attacking);
}

void UGA_Jump::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
	EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 실제 점프 트리거. 접지/연료 등 가능 여부는 CharacterMovement::CanJump가 판단한다(공중이면 무시 → 더블점프 방지).
	Character->Jump();

	// 점프 입력만 던지고 즉시 종료 — 이후 궤적은 CharacterMovement가 이어서 처리한다.
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}