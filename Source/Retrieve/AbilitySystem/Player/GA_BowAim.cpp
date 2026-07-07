
#include "GA_BowAim.h"

#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Components/Combat/CombatStanceComponent.h"
#include "Components/LockOn/LockOnComponent.h"
#include "Components/Player/WeaponComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"

namespace
{
	const FName BowAimCameraOverrideId(TEXT("BowAim"));
}

UGA_BowAim::UGA_BowAim()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ERetrieveAbilityActivationPolicy::WhileInputActive;

	BowAimCameraProfile.TargetArmLength = 300.f;
	BowAimCameraProfile.CameraRelativeOffset = FVector(0.f, 90.f, 25.f);
	BowAimCameraProfile.ArmBlendSpeed = 12.f;
	BowAimCameraProfile.OffsetBlendSpeed = 10.f;

	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_BowAim);
	SetAssetTags(Tags);

	bBlockActivationWhileAirborne = true;

	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Player_Aiming);

	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dodging);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Knockdown);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dead);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_ForcedKnockback);
}

bool UGA_BowAim::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!IsValid(AvatarActor))
	{
		return false;
	}

	const UWeaponComponent* WeaponComp = AvatarActor ? AvatarActor->FindComponentByClass<UWeaponComponent>() : nullptr;
	if (!IsValid(WeaponComp))
	{
		return false;
	}

	if (!WeaponComp->IsEquipped())
	{
		return false;
	}

	const FGameplayTag WeaponTypeTag = WeaponComp->GetWeaponDataRef().WeaponTypeTag;
	if (WeaponTypeTag != RetrieveGameplayTags::Weapon_Type_Bow)
	{
		return false;
	}

	return true;
}

void UGA_BowAim::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	WaitInputReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, false);
	if (!IsValid(WaitInputReleaseTask))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ActorInfo && ActorInfo->IsLocallyControlled())
	{
		AActor* AvatarActor = ActorInfo->AvatarActor.Get();
		
		if (ULockOnComponent* LockOn = IsValid(AvatarActor) ? AvatarActor->FindComponentByClass<ULockOnComponent>() : nullptr)
		{
			LockOn->StopLockOn();
		}
		
		ActiveCameraBoom = IsValid(AvatarActor) ? AvatarActor->FindComponentByClass<URetrieveCameraBoom>() : nullptr;
		if (ActiveCameraBoom.IsValid())
		{
			ActiveCameraBoom->SetCameraBoomProfileOverride(BowAimCameraOverrideId, BowAimCameraProfile);
		}
	}

	// 조준 시작 = 전투 활동으로 신고 → 기존 발검 파이프라인이 UnSheathe 몽타주를 재생한다.
	// (로컬/서버 양쪽에서 태그·소켓이 일치하도록 IsLocallyControlled 블록 밖에서 호출)
	if (AActor* AvatarActor = GetAvatarActorFromActorInfo())
	{
		if (UCombatStanceComponent* Stance = AvatarActor->FindComponentByClass<UCombatStanceComponent>())
		{
			Stance->NotifyCombatActivity(/*bFromAttack=*/false); // false → 발검 몽타주 재생
		}
	}

	WaitInputReleaseTask->OnRelease.AddDynamic(this, &ThisClass::HandleInputReleased);
	WaitInputReleaseTask->ReadyForActivation();
}

void UGA_BowAim::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                            const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (IsValid(WaitInputReleaseTask))
	{
		WaitInputReleaseTask->EndTask();
		WaitInputReleaseTask = nullptr;
	}

	if (ActiveCameraBoom.IsValid())
	{
		ActiveCameraBoom->ClearCameraBoomProfileOverride(BowAimCameraOverrideId);
		ActiveCameraBoom.Reset();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_BowAim::HandleInputReleased(float TimeHeld)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
