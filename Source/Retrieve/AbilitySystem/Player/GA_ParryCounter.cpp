#include "AbilitySystem/Player/GA_ParryCounter.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "GameplayEffect.h"
#include "AbilitySystem/Player/GA_Guard.h"
#include "Combat/RetrieveTargetingLibrary.h"
#include "Components/CombatReactionComponent.h"
#include "Components/WeaponComponent.h"
#include "Data/AttackComboDefinition.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Logging/RetrieveLogChannels.h"

UGA_ParryCounter::UGA_ParryCounter()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_ParryCounter);
	SetAssetTags(Tags);

	ActivationRequiredTags.AddTag(RetrieveGameplayTags::State_Player_CanCounter);

	bBlockActivationWhileAirborne = true;

	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dead);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Knockdown);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dodging);

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

	// 콤보 정의에서 현재 원소의 ParryCounter variant 해결 (없으면 기본 variant)
	UAttackComboDefinition* ComboDefinition = CachedWeaponData.AttackComboDefinition.LoadSynchronous();
	const FParryCounterData* ResolvedParry = ComboDefinition ? ComboDefinition->ResolveParryVariant(ResolveCurrentElementTag()) : nullptr;
	if (!ResolvedParry)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}
	CachedParryData = *ResolvedParry;

	if (AActor* Target = ResolveCounterTarget())
	{
		ApplyCounterToTarget(Target);
	}

	UAnimMontage* Montage = CachedParryData.CounterMontage.LoadSynchronous();
	if (!IsValid(Montage))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, Montage, 1.f, NAME_None, true);
	if (!MontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageCancelled);
	MontageTask->ReadyForActivation();
}

AActor* UGA_ParryCounter::ResolveCounterTarget() const
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor))
	{
		return nullptr;
	}

	// 1) GA_Guard가 캐싱한 패리 대상
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (IsValid(ASC))
	{
		TArray<FGameplayAbilitySpec*> GuardSpecs;
		FGameplayTagContainer GuardFilter;
		GuardFilter.AddTag(RetrieveGameplayTags::Ability_Player_Guard);
		ASC->GetActivatableGameplayAbilitySpecsByAllMatchingTags(GuardFilter, GuardSpecs, false);

		for (const FGameplayAbilitySpec* Spec : GuardSpecs)
		{
			if (!Spec) continue;
			for (UGameplayAbility* Instance : Spec->GetAbilityInstances())
			{
				if (UGA_Guard* Guard = Cast<UGA_Guard>(Instance))
				{
					if (AActor* ParriedActor = Guard->GetLastParriedAttacker())
					{
						return ParriedActor;
					}
				}
			}
		}
	}

	// 2) 락온 대상
	if (const UCombatReactionComponent* CombatReaction = AvatarActor->FindComponentByClass<UCombatReactionComponent>())
	{
		if (AActor* LockOnTarget = CombatReaction->GetLockOnTarget())
		{
			return LockOnTarget;
		}
	}

	// 3) 전방 콘 검색
	ACharacter* SourceChar = Cast<ACharacter>(AvatarActor);
	if (!IsValid(SourceChar))
	{
		return nullptr;
	}
	const FVector Aim = SourceChar->GetControlRotation().Vector();
	return URetrieveTargetingLibrary::FindBestTarget(SourceChar, 500.f, 90.f, Aim, 200.f, 0.25f);
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

	FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
	Ctx.AddInstigator(AvatarActor, AvatarActor);
	Ctx.AddSourceObject(this);

	FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(), Ctx);
	if (Spec.IsValid() && Spec.Data.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Damage_Mul, CachedParryData.DamageMultiplier);
		Spec.Data->AddDynamicAssetTag(RetrieveGameplayTags::Attack_Type_Normal);

		if (const FGameplayTag ReactTag = HitReactTypeToTag(CachedParryData.HitReactType); ReactTag.IsValid())
		{
			Spec.Data->AddDynamicAssetTag(ReactTag);
		}

		SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	}

	ApplyGroggyToTarget(TargetActor, TargetASC);

	UE_LOG(LogRetrieveCombat, Log, TEXT("[ParryCounter] Counter applied to %s (DamageMul=%.2f)"),
		*GetNameSafe(TargetActor), CachedParryData.DamageMultiplier);
}

void UGA_ParryCounter::ApplyGroggyToTarget(AActor* TargetActor, UAbilitySystemComponent* TargetASC) const
{
	if (!IsValid(TargetActor) || !IsValid(TargetASC))
	{
		return;
	}

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(AvatarActor) || !IsValid(SourceASC))
	{
		return;
	}

	const bool bIsBoss = TargetASC->HasMatchingGameplayTag(RetrieveGameplayTags::Monster_Type_Boss);

	TSubclassOf<UGameplayEffect> GroggyGE = bIsBoss
		? CachedParryData.BossGroggyEffect
		: CachedParryData.NormalGroggyEffect;

	if (!GroggyGE)
	{
		return;
	}

	FGameplayEffectContextHandle GroggyCtx = SourceASC->MakeEffectContext();
	GroggyCtx.AddInstigator(AvatarActor, AvatarActor);
	GroggyCtx.AddSourceObject(this);

	FGameplayEffectSpecHandle GroggySpec = SourceASC->MakeOutgoingSpec(GroggyGE, GetAbilityLevel(), GroggyCtx);
	if (!GroggySpec.IsValid() || !GroggySpec.Data.IsValid())
	{
		return;
	}

	const float Duration = CachedParryData.GroggyDuration;
	if (GroggyDurationTag.IsValid() && Duration > 0.f)
	{
		GroggySpec.Data->SetSetByCallerMagnitude(GroggyDurationTag, Duration);
	}

	SourceASC->ApplyGameplayEffectSpecToTarget(*GroggySpec.Data.Get(), TargetASC);

	UE_LOG(LogRetrieveCombat, Log, TEXT("[ParryCounter] Groggy applied to %s (Boss=%d, Duration=%.1f)"),
		*GetNameSafe(TargetActor), bIsBoss ? 1 : 0, Duration);
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
}

void UGA_ParryCounter::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	StopRuntimeTasks();

	CachedWeaponComponent = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_ParryCounter::CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility)
{
	StopRuntimeTasks();
	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}
