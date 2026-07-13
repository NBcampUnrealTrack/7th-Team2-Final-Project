#include "AbilitySystem/Player/GA_Dash.h"

#include "AbilitySystem/Attributes/CombatAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Character/RetrieveAlsCharacter.h"
#include "Components/Player/RetrieveHeroComponent.h"
#include "Components/Player/WeaponComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "GameplayTags/RetrieveGameplayTags.h"

UGA_Dash::UGA_Dash()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	
	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_Dash);
	SetAssetTags(Tags);

	// 공중/점프 입력 중에는 대시 불가 (베이스 공용 게이트)
	bBlockActivationWhileAirborne = true;

	// 회피 중 상태 태그
	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Player_Dodging);

	// 경직/다운/사망 중에는 발동 불가
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Knockdown);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dead);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_ForcedKnockback);
	
	// 재대시 명시적 차단
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dodging);

	// 공격(family)/가드 즉시 취소하고 발동
	CancelAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Type_Attack);
	CancelAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Guard);

	StaminaCostTag = RetrieveGameplayTags::Ability_Player_Dash;
}

bool UGA_Dash::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	
	const AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const UWeaponComponent* WeaponComp = AvatarActor ? AvatarActor->FindComponentByClass<UWeaponComponent>() : nullptr;
	if (WeaponComp && WeaponComp->GetWeaponDataRef().WeaponTypeTag == RetrieveGameplayTags::Weapon_Type_Staff)
	{
		return false;
	}
	
	return true;
}

void UGA_Dash::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!IsValid(AvatarActor))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	// ALS StartRollingImplementation과 동일 흐름:
	// RollingState.TargetYawAngle + SetRotationInstant + SetLocomotionAction(Rolling) 일괄 처리.
	// 이게 없으면 매 프레임 RefreshRollingPhysics가 default TargetYawAngle(보통 0)로
	// 캐릭터를 끌고 가서 의도하지 않은 방향으로 굴러갑니다.
	const FVector DashDir = ResolveDashDirection(ActorInfo);
	if (!DashDir.IsNearlyZero())
	{
		if (ARetrieveAlsCharacter* Als = Cast<ARetrieveAlsCharacter>(AvatarActor))
		{
			Als->BeginRollLockoutTowardYaw(DashDir.Rotation().Yaw);
		}
	}

	UAnimMontage* Montage = DashMontage.LoadSynchronous();
	if (!IsValid(Montage))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 최종 PlayRate = BaseDashPlayRate × (MoveSpeed Attribute / 기준값)
	float FinalPlayRate = BaseDashPlayRate;
	if (const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		if (const UCombatAttributeSet* AttrSet = ASC->GetSet<UCombatAttributeSet>())
		{
			FinalPlayRate *= (AttrSet->GetMoveSpeed() / UCombatAttributeSet::ReferenceMoveSpeed);
		}
	}

	// bAllowInterruptAfterBlendOut=true: 블렌드아웃 중 hit react 등 다른 몽타주가 끼어들어도
	// OnInterrupted가 발동되어 EndAbility까지 도달하도록 보장(미설정 시 콜백 누락 → 종료 안 됨).
	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, Montage, FinalPlayRate, NAME_None, /*bStopWhenAbilityEnds=*/true,
		1.f, 0.f, /*bAllowInterruptAfterBlendOut=*/true);
	if (!MontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageFinished);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageFinished);
	MontageTask->ReadyForActivation();
}

FVector UGA_Dash::ResolveDashDirection(const FGameplayAbilityActorInfo* ActorInfo) const
{
	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!IsValid(AvatarActor))
	{
		return FVector::ZeroVector;
	}
	
	if (const URetrieveHeroComponent* Hero = URetrieveHeroComponent::FindHeroComponent(AvatarActor))
	{
		FVector Cached = Hero->GetCachedMoveInputDirection();
		Cached.Z = 0.f;
		if (!Cached.IsNearlyZero())
		{
			return Cached.GetSafeNormal();
		}
	}
	
	FVector Forward = AvatarActor->GetActorForwardVector();
	Forward.Z = 0.f;
	return Forward.GetSafeNormal();
}

void UGA_Dash::HandleMontageFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}

bool UGA_Dash::OpenNotifyIFrameWindow()
{
	if (bIFrameWindowOpened || !IFrameWindowEffect)
	{
		return false;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(ASC))
	{
		return false;
	}

	const FGameplayEffectSpecHandle Spec = MakeSourcedSpec(IFrameWindowEffect, GetAbilityLevel());
	if (!Spec.IsValid() || !Spec.Data.IsValid())
	{
		return false;
	}

	IFrameWindowHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	if (!IFrameWindowHandle.IsValid())
	{
		return false;
	}

	bIFrameWindowOpened = true;
	return true;
}

void UGA_Dash::CloseNotifyIFrameWindow()
{
	// ANS End와 EndAbility가 모두 닫기를 시도할 수 있으므로 idempotent하게 둔다.
	if (!bIFrameWindowOpened && !IFrameWindowHandle.IsValid())
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (IFrameWindowHandle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(IFrameWindowHandle);
		}
	}

	IFrameWindowHandle.Invalidate();
	bIFrameWindowOpened = false;
}

void UGA_Dash::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 어떤 종료 경로에서도 무적 GE 제거(i-frame 도중 캔슬 시 무적 영구 잔류 방지).
	CloseNotifyIFrameWindow();

	// 모든 종료 경로(완주/인터럽트/외부 캔슬)에서 ALS Rolling 락아웃 해제. SetLocomotionAction(Empty)는 멱등.
	if (ARetrieveAlsCharacter* Als = Cast<ARetrieveAlsCharacter>(GetAvatarActorFromActorInfo()))
	{
		Als->EndRollLockout();
	}

	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
