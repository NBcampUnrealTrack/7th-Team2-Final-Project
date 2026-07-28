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
#include "Components/Pawn/RetrieveCameraBoom.h"
#include "GameFramework/PlayerController.h"
#include "Components/Enemy/EpicMonsterGroggyComponent.h"
#include "Components/LockOn/LockOnComponent.h"
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
	// 카운터 진행 동안 무적(전투 피해 무시).
	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Player_Invincible);

	CancelAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Guard);

	// 카운터 몽타주가 끝날 때까지 다른 입력으로 끊기지 않게 주요 전투 입력을 막는다(어빌리티 수명=몽타주 수명).
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Type_Attack);
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Guard);
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Parry);
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Dash);
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Blink);
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Burst);
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

	const FParryCounterData* CounterData = ComboDefinition->ResolveParryVariant(ResolveCurrentElementTag());
	if (!CounterData)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}
	CachedParryCounterData = *CounterData;

	UAnimMontage* Montage = CachedParryCounterData.CounterMontage.LoadSynchronous();
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

	AActor* Avatar = GetAvatarActorFromActorInfo();

	// 카운터 전용 카메라 구도와 충돌하지 않게 락온을 푼다.
	if (ULockOnComponent* LockOn = Avatar ? Avatar->FindComponentByClass<ULockOnComponent>() : nullptr)
	{
		if (LockOn->IsLockedOn())
		{
			LockOn->StopLockOn();
		}
	}

	// 타겟 뒤 구도로 블렌드 + 룩 잠금. EndAbility에서 원래 시점으로 복귀.
	APlayerController* PC = CurrentActorInfo ? CurrentActorInfo->PlayerController.Get() : nullptr;
	UCounterTimeDilationComponent* CameraComp = Avatar ? Avatar->FindComponentByClass<UCounterTimeDilationComponent>() : nullptr;
	if (IsValid(PC) && IsValid(CameraComp))
	{
		FRotator FramingRot = PC->GetControlRotation();
		const FVector ToTarget = (CachedCounterTarget->GetActorLocation() - Avatar->GetActorLocation()).GetSafeNormal2D();
		if (!ToTarget.IsNearlyZero())
		{
			FramingRot = FRotator(ComboDefinition->Parry.CounterCameraPitch,
				ToTarget.Rotation().Yaw + ComboDefinition->Parry.CounterCameraYawOffset, 0.f);
		}
		CameraComp->BeginCounterCamera(PC, FramingRot, CounterCameraBlendSpeed);
	}

	// 창당 1회 발행되는 Impact.Begin을 구독 → ANS_AttackImpact 창 하나 = 카운터 히트 하나(찌르기/내려치기 2히트).
	ImpactEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, RetrieveGameplayTags::GameplayEvent_Attack_Impact_Begin, nullptr, false, true);
	if (ImpactEventTask)
	{
		ImpactEventTask->EventReceived.AddDynamic(this, &ThisClass::HandleImpactEvent);
		ImpactEventTask->ReadyForActivation();
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

void UGA_ParryCounter::ApplyCounterToTarget(AActor* TargetActor, float DamageScale, bool bFirstHit)
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

	const bool bTargetIsBoss = TargetASC->HasMatchingGameplayTag(RetrieveGameplayTags::Monster_Type_Boss);
	const float DamageMul = (bTargetIsBoss ? CachedParryCounterData.BossDamageMultiplier : CachedParryCounterData.NormalDamageMultiplier) * DamageScale;

	FGameplayEffectSpecHandle Spec = MakeSourcedSpec(DamageEffectClass, GetAbilityLevel());
	if (Spec.IsValid() && Spec.Data.IsValid())
	{
		// 임팩트 파티클(GameplayCue.Combat.Hit)은 컨텍스트 HitResult 위치에 뜬다. 카운터는 트레이스가 없어
		// 대상 위치를 HitResult로 직접 넣어야 파티클이 (0,0,0)이 아닌 대상에 표시된다(일반 공격과 동일).
		FHitResult CounterHit;
		CounterHit.Location = TargetActor->GetActorLocation();
		CounterHit.ImpactPoint = TargetActor->GetActorLocation();
		Spec.Data->GetContext().AddHitResult(CounterHit, /*bReset=*/true);

		Spec.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Damage_Mul, DamageMul);
		// 넉백은 마무리 히트(내려치기)에만. 찌르기(첫 히트)는 밀치지 않는다(전역 기본 넉백도 태그로 차단).
		if (!bFirstHit && CachedParryCounterData.KnockbackStrength > 0.f)
		{
			Spec.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Knockback_Strength, CachedParryCounterData.KnockbackStrength);
			Spec.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Knockback_UpwardStrength, CachedParryCounterData.KnockbackUpwardStrength);
		}
		else
		{
			Spec.Data->AddDynamicAssetTag(RetrieveGameplayTags::Attack_Property_NoKnockback);
		}
		Spec.Data->AddDynamicAssetTag(RetrieveGameplayTags::Attack_Type_Normal);

		if (const FGameplayTag ReactTag = HitReactTypeToTag(CachedParryCounterData.HitReactType); ReactTag.IsValid())
		{
			Spec.Data->AddDynamicAssetTag(ReactTag);
		}

		AddCombatTagsToDamageSpec(
			*Spec.Data.Get(),
			ResolveCurrentElementTag(),
			RetrieveGameplayTags::Attack_Type_Normal,
			FGameplayTag(),
			HitReactTypeToTag(CachedParryCounterData.HitReactType));

		const FGameplayTag HitSuccessTag = CachedParryCounterData.HitSuccessFeedbackTag.IsValid()
		? CachedParryCounterData.HitSuccessFeedbackTag
		: RetrieveGameplayTags::GameplayEvent_Attack_HitSuccess_Heavy;

		const FGameplayTag TargetHitTag = CachedParryCounterData.TargetHitFeedbackTag.IsValid()
		? CachedParryCounterData.TargetHitFeedbackTag
		: RetrieveGameplayTags::GameplayEvent_Hit_Heavy;

		Spec.Data->AddDynamicAssetTag(HitSuccessTag);
		Spec.Data->AddDynamicAssetTag(TargetHitTag);

		SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	}

	UE_LOG(LogRetrieveCombat, Log, TEXT("[ParryCounter] Hit applied to %s (DamageMul=%.2f, Scale=%.2f)"),
		*GetNameSafe(TargetActor), DamageMul, DamageScale);

	// 그로기는 첫 히트에만.
	if (!bFirstHit)
	{
		return;
	}

	// 그로기: 대상 타입별 GE(있으면). 없으면 몬스터 그로기 컴포넌트로 폴백.
	if (const TSubclassOf<UGameplayEffect> GroggyGE = bTargetIsBoss ? CachedParryCounterData.BossGroggyEffect : CachedParryCounterData.NormalGroggyEffect)
	{
		const FGameplayEffectSpecHandle GroggySpec = MakeSourcedSpec(GroggyGE, GetAbilityLevel());
		if (GroggySpec.IsValid() && GroggySpec.Data.IsValid())
		{
			SourceASC->ApplyGameplayEffectSpecToTarget(*GroggySpec.Data.Get(), TargetASC);
		}
	}
	else
	{
		TryApplyMonsterGroggy(TargetActor, CachedParryCounterData.GroggyDuration);
	}
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

void UGA_ParryCounter::HandleImpactEvent(FGameplayEventData Payload)
{
	if (!IsActive())
	{
		return;
	}

	// EventMagnitude = 이 창의 ANS_AttackImpact.DamageScale.
	const float HitDamageScale = Payload.EventMagnitude > 0.f ? Payload.EventMagnitude : 1.f;
	const bool bFirstHit = (CounterHitIndex == 0);
	++CounterHitIndex;
	ApplyCounterToTarget(CachedCounterTarget.Get(), HitDamageScale, bFirstHit);
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

	// 카운터 카메라 원복(복귀 완료 시 컴포넌트가 룩 잠금 해제).
	if (AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		if (UCounterTimeDilationComponent* CameraComp = Avatar->FindComponentByClass<UCounterTimeDilationComponent>())
		{
			CameraComp->EndCounterCamera();
		}
	}

	CachedWeaponComponent = nullptr;
	CachedCounterTarget.Reset();
	CounterHitIndex = 0;

	// 줌 복귀 안전장치: 원복 노티가 없거나 카운터가 중단돼도 유저 카메라 거리로 돌아오게 한다(프로파일 해제).
	if (AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		if (URetrieveCameraBoom* Boom = Avatar->FindComponentByClass<URetrieveCameraBoom>())
		{
			Boom->ClearCameraBoomProfileOverride(FName(TEXT("ParryCounter")));
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_ParryCounter::CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility)
{
	StopRuntimeTasks();
	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}
