
#include "GA_BowAim.h"

#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "Components/Combat/CombatStanceComponent.h"
#include "Components/LockOn/LockOnComponent.h"
#include "Components/Player/WeaponComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Settings/RetrieveGameUserSettings.h"

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

	// 달리기 ↔ 조준 상호배타: 조준이 실제 발동하면 달리기를 끈다(입력 태그 무관, 활 조건은 CanActivate가 보장).
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->SetLooseGameplayTagCount(RetrieveGameplayTags::State_Player_Sprinting, 0);
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

	if (IsValid(WaitInputPressTask))
	{
		WaitInputPressTask->EndTask();
		WaitInputPressTask = nullptr;
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
	// WaitInputRelease 태스크는 발동 시 한 번만 만들어지므로 여기서 처리하고 나면 재사용하지 않는다.
	WaitInputReleaseTask = nullptr;

	const URetrieveGameUserSettings* Settings = URetrieveGameUserSettings::Get();
	if (Settings && Settings->bBowAimToggleMode)
	{
		// 토글 모드: 우클릭을 떼도 조준을 유지하고, 다음 우클릭에서 해제한다.
		WaitInputPressTask = UAbilityTask_WaitInputPress::WaitInputPress(this);
		if (IsValid(WaitInputPressTask))
		{
			WaitInputPressTask->OnPress.AddDynamic(this, &ThisClass::HandleToggleOffInputPressed);
			WaitInputPressTask->ReadyForActivation();
			return;
		}
		// 태스크 생성 실패 시 홀드 모드로 안전하게 폴백.
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_BowAim::HandleToggleOffInputPressed(float TimeWaited)
{
	// 이 콜백은 우클릭을 아직 떼기 전(같은 프레임)에 불린다. ASC의 InputHeldSpecHandles에
	// 이 스펙이 남아있으면 WhileInputActive 재시도가 바로 이어서 재발동시켜 "꺼지지 않는" 것처럼 보인다.
	// EndAbility 전에 held 상태를 미리 지워 재발동을 막는다.
	if (URetrieveAbilitySystemComponent* ASC = Cast<URetrieveAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo()))
	{
		ASC->ClearInputHeldForSpec(CurrentSpecHandle);
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
