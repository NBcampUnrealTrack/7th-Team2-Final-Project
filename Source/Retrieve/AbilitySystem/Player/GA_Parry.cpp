#include "AbilitySystem/Player/GA_Parry.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Components/Player/WeaponComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameplayTags/RetrieveGameplayTags.h"

UGA_Parry::UGA_Parry()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;

	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_Parry);
	SetAssetTags(Tags);

	// 공중/회피·경직·다운·사망 중 발동 차단(플레이어 액션 공통 게이트)
	ApplyCommonActionBlocks();

	StaminaCostTag = RetrieveGameplayTags::Ability_Player_Parry;
}

bool UGA_Parry::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// 방패는 GA_Guard가 담당(홀드 블록 + 진입 패링).
	const AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const UWeaponComponent* WeaponComp = AvatarActor ? AvatarActor->FindComponentByClass<UWeaponComponent>() : nullptr;
	if (WeaponComp && WeaponComp->GetWeaponDataRef().WeaponTypeTag == RetrieveGameplayTags::Weapon_Type_SwordShield)
	{
		return false;
	}

	// 무기가 패링 가능해야 발동(Parry.SuccessMontage 존재). 활 등 미설정 무기는 R 무반응.
	return WeaponCanParry();
}

void UGA_Parry::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 성공 이벤트를 먼저 구독. 창은 시도 몽타주의 ANS_ParryWindow가 연다.
	StartListeningForParrySuccess();

	const FWeaponParryData* ParryData = ResolveParryData();
	UAnimMontage* Montage = ParryData ? ParryData->ParryMontage.LoadSynchronous() : nullptr;
	if (!Montage)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, Montage, ParryData->ParryMontagePlayRate, NAME_None, /*bStopWhenAbilityEnds=*/true);
	if (!MontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageFinished);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageFinished);
	MontageTask->ReadyForActivation();
}

bool UGA_Parry::OpenNotifyParryWindow()
{
	return OpenParryWindow();
}

void UGA_Parry::CloseNotifyParryWindow()
{
	CloseParryWindow();
}

void UGA_Parry::HandleMontageFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}

void UGA_Parry::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
