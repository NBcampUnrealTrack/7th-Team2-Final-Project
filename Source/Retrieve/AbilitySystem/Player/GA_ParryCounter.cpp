#include "AbilitySystem/Player/GA_ParryCounter.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "GameplayEffect.h"
#include "MotionWarpingComponent.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Combat/RetrieveTargetingLibrary.h"
#include "Components/Enemy/EpicMonsterGroggyComponent.h"
#include "Components/Player/CounterTimeDilationComponent.h"
#include "Components/Player/WeaponComponent.h"
#include "Data/WeaponAttackDefinition.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Logging/RetrieveLogChannels.h"

namespace
{
const FName& GetCounterWarpTargetName()
{
	static const FName CounterWarpTargetName(TEXT("AttackTarget"));
	return CounterWarpTargetName;
}
}

UGA_ParryCounter::UGA_ParryCounter()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_ParryCounter);
	Tags.AddTag(RetrieveGameplayTags::Ability_Type_Attack);
	SetAssetTags(Tags);
	
	FAbilityTriggerData CounterTrigger;
	CounterTrigger.TriggerTag = RetrieveGameplayTags::GameplayEvent_Parry_Counter;
	CounterTrigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(CounterTrigger);

	// 공중/회피·경직·다운·사망 중 발동 차단(플레이어 액션 공통 게이트)
	ApplyCommonActionBlocks();

	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Player_Attacking);
	
	CancelAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Guard);
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Guard);
}

bool UGA_ParryCounter::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const UWeaponComponent* WeaponComp = AvatarActor ? AvatarActor->FindComponentByClass<UWeaponComponent>() : nullptr;
	if (!IsValid(WeaponComp) || !WeaponComp->IsEquipped())
	{
		return false;
	}

	if (WeaponComp->GetWeaponDataRef().AttackComboDefinition.IsNull())
	{
		return false;
	}

	return IsValid(DamageEffectClass);
}

void UGA_ParryCounter::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	CachedWeaponComponent = AvatarActor ? AvatarActor->FindComponentByClass<UWeaponComponent>() : nullptr;
	if (!IsValid(CachedWeaponComponent))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	CachedWeaponData = CachedWeaponComponent->GetWeaponDataRef();

	UWeaponAttackDefinition* ComboDefinition = CachedWeaponData.AttackComboDefinition.LoadSynchronous();
	if (!IsValid(ComboDefinition))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}
	
	UAnimMontage* Montage = ComboDefinition->ParrySuccessMontage.LoadSynchronous();
	if (!IsValid(Montage))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	CachedCounterTarget = ResolveCounterTarget();
	if (!CachedCounterTarget.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	RegisterCounterWarpTarget();

	ImpactEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, RetrieveGameplayTags::GameplayEvent_Attack_Impact, nullptr, false, true);
	if (ImpactEventTask)
	{
		ImpactEventTask->EventReceived.AddDynamic(this, &ThisClass::HandleImpactEvent);
		ImpactEventTask->ReadyForActivation();
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, Montage, ComboDefinition->ParrySuccessMontagePlayRate, NAME_None, true);
	if (!MontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageCancelled);

	// 카운터 대시 몽타주가 확정된 뒤 reboost에 진입한다.
	// 몽타주가 없으면 WindowSlow 만료 복구 경로를 그대로 둔다.
	if (AActor* CounterAvatar = GetAvatarActorFromActorInfo())
	{
		if (UCounterTimeDilationComponent* TimeComp = CounterAvatar->FindComponentByClass<UCounterTimeDilationComponent>())
		{
			TimeComp->EnterReboost();
		}
	}

	MontageTask->ReadyForActivation();
}

AActor* UGA_ParryCounter::ResolveCounterTarget() const
{
	const URetrieveAbilitySystemComponent* RetrieveASC = Cast<URetrieveAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
	if (!IsValid(RetrieveASC))
	{
		return nullptr;
	}

	return RetrieveASC->GetPendingCounterTarget();
}

void UGA_ParryCounter::RegisterCounterWarpTarget()
{
	ACharacter* SourceCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	AActor* TargetActor = CachedCounterTarget.Get();
	if (!IsValid(SourceCharacter) || !IsValid(TargetActor))
	{
		return;
	}

	UMotionWarpingComponent* MotionWarping = SourceCharacter->FindComponentByClass<UMotionWarpingComponent>();
	if (!IsValid(MotionWarping))
	{
		return;
	}

	const FTransform WarpTransform = URetrieveTargetingLibrary::BuildWarpTransform(
		SourceCharacter,
		TargetActor,
		CounterWarpStandoffOffset,
		CounterMaxWarpDistance);

	const FName CounterWarpTargetName = GetCounterWarpTargetName();
	MotionWarping->AddOrUpdateWarpTargetFromTransform(CounterWarpTargetName, WarpTransform);

	if (URetrieveAbilitySystemComponent* RetrieveASC = Cast<URetrieveAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo()))
	{
		RetrieveASC->SetCounterWarpTargetLocked(true);
	}
}

void UGA_ParryCounter::ClearCounterWarpTarget()
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor))
	{
		return;
	}

	if (UMotionWarpingComponent* MotionWarping = AvatarActor->FindComponentByClass<UMotionWarpingComponent>())
	{
		const FName CounterWarpTargetName = GetCounterWarpTargetName();
		MotionWarping->RemoveWarpTarget(CounterWarpTargetName);
	}
}

void UGA_ParryCounter::ApplyCounterToTarget(AActor* TargetActor)
{
	if (!HasAuthority(&GetCurrentActivationInfoRef()))
	{
		return;
	}

	if (!IsValid(TargetActor))
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(SourceASC) || !IsValid(AvatarActor) || !IsValid(DamageEffectClass))
	{
		return;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (!IsValid(TargetASC))
	{
		return;
	}

	FGameplayEffectSpecHandle Spec = MakeSourcedSpec(DamageEffectClass, GetAbilityLevel());
	if (Spec.IsValid() && Spec.Data.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Damage_Mul, CounterDamageMultiplier);
		if (CounterKnockbackStrength > 0.f)
		{
			Spec.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Knockback_Strength, CounterKnockbackStrength);
			Spec.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Knockback_UpwardStrength, CounterKnockbackUpwardStrength);
		}
		Spec.Data->AddDynamicAssetTag(RetrieveGameplayTags::Attack_Type_Normal);

		if (const FGameplayTag ReactTag = HitReactTypeToTag(CounterHitReactType); ReactTag.IsValid())
		{
			Spec.Data->AddDynamicAssetTag(ReactTag);
		}

		AddCombatTagsToDamageSpec(
			*Spec.Data.Get(),
			ResolveCurrentElementTag(),
			RetrieveGameplayTags::Attack_Type_Normal,
			FGameplayTag(),
			HitReactTypeToTag(CounterHitReactType));

		const FGameplayTag HitSuccessTag = CounterHitSuccessFeedbackTag.IsValid()
		? CounterHitSuccessFeedbackTag
		: RetrieveGameplayTags::GameplayEvent_Attack_HitSuccess_Heavy;

		const FGameplayTag TargetHitTag = CounterTargetHitFeedbackTag.IsValid()
		? CounterTargetHitFeedbackTag
		: RetrieveGameplayTags::GameplayEvent_Hit_Heavy;

		Spec.Data->AddDynamicAssetTag(HitSuccessTag);
		Spec.Data->AddDynamicAssetTag(TargetHitTag);

		SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	}

	if (bApplyGroggyOnImpact)
	{
		TryApplyMonsterGroggy(TargetActor, CounterGroggyDuration);
	}

	UE_LOG(LogRetrieveCombat, Log, TEXT("[ParryCounter] Counter applied to %s (DamageMul=%.2f)"),
		*GetNameSafe(TargetActor), CounterDamageMultiplier);
}

bool UGA_ParryCounter::TryApplyMonsterGroggy(AActor* TargetActor, float Duration) const
{
	if (!IsValid(TargetActor) || Duration <= 0.f)
	{
		return false;
	}

	if (UEpicMonsterGroggyComponent* GroggyComp = TargetActor->FindComponentByClass<UEpicMonsterGroggyComponent>())
	{
		GroggyComp->ApplyGroggyState(Duration);
		return true;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (!IsValid(TargetASC))
	{
		return false;
	}

	FGameplayEventData GroggyEvent;
	GroggyEvent.EventTag = RetrieveGameplayTags::GameplayEvent_GroggyTrigger;
	GroggyEvent.Instigator = GetAvatarActorFromActorInfo();
	GroggyEvent.Target = TargetActor;
	GroggyEvent.EventMagnitude = Duration;

	TargetASC->HandleGameplayEvent(RetrieveGameplayTags::GameplayEvent_GroggyTrigger, &GroggyEvent);
	return true;
}

void UGA_ParryCounter::HandleImpactEvent(FGameplayEventData /*Payload*/)
{
	if (!IsActive() || bCounterImpactApplied)
	{
		return;
	}

	bCounterImpactApplied = true;
	ApplyCounterToTarget(CachedCounterTarget.Get());
}

void UGA_ParryCounter::HandleMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_ParryCounter::HandleMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_ParryCounter::HandleMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_ParryCounter::StopRuntimeTasks()
{
	if (MontageTask) { MontageTask->EndTask(); MontageTask = nullptr; }
	if (ImpactEventTask) { ImpactEventTask->EndTask(); ImpactEventTask = nullptr; }
}

void UGA_ParryCounter::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	StopRuntimeTasks();
	ClearCounterWarpTarget();

	if (URetrieveAbilitySystemComponent* RetrieveASC = Cast<URetrieveAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo()))
	{
		RetrieveASC->SetCounterWarpTargetLocked(false);
		RetrieveASC->ClearPendingCounterTarget();
	}

	CachedWeaponComponent = nullptr;
	CachedCounterTarget.Reset();
	bCounterImpactApplied = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_ParryCounter::CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility)
{
	StopRuntimeTasks();
	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}
