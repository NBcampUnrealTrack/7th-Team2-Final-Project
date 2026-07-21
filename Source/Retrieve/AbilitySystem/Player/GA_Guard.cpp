#include "AbilitySystem/Player/GA_Guard.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Audio/RetrieveMusicSubsystem.h"
#include "Components/Player/WeaponComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "TimerManager.h"

UGA_Guard::UGA_Guard()
{
	InstancingPolicy  = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ReplicationPolicy  = EGameplayAbilityReplicationPolicy::ReplicateYes;

	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_Guard);
	SetAssetTags(Tags);

	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Player_Guarding);
	ActivationPolicy = ERetrieveAbilityActivationPolicy::WhileInputActive;
	
	// 공중/회피·경직·다운·사망 중 가드 차단(플레이어 액션 공통 게이트)
	ApplyCommonActionBlocks();

	// Guard 중 일반 공격류는 막되, GuardAttack은 예외로 통과시킨다.
	// GuardAttack도 Ability.Type.Attack을 갖기 때문에 Ability.Type.Attack 전체를 막으면
	// Guard 중 Attack 입력이 GuardAttack으로 치환되어도 발동 전에 차단된다.
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Attack);
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_HeavyAttack);
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_SprintAttack);
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_JumpAttack);
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_BowShot);

	StaminaCostTag = RetrieveGameplayTags::Ability_Player_Guard;
}

bool UGA_Guard::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// 막기는 방패 전용 (그 외 무기는 GA_Parry). 스태미너 게이팅은 베이스 CheckCost(DT 조회)가 처리.
	const AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const UWeaponComponent* WeaponComp = AvatarActor ? AvatarActor->FindComponentByClass<UWeaponComponent>() : nullptr;
	return WeaponComp &&
		WeaponComp->GetWeaponDataRef().WeaponTypeTag == RetrieveGameplayTags::Weapon_Type_SwordShield;
}

void UGA_Guard::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 달리기 ↔ 가드 상호배타: 가드가 실제 발동하면 달리기를 끈다(입력 태그 무관, 무기 조건은 CanActivate가 이미 보장).
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->SetLooseGameplayTagCount(RetrieveGameplayTags::State_Player_Sprinting, 0);
	}

	// 가드 진입부 패링 판정 구독. 실제 창은 GuardMontage의 ANS_ParryWindow가 연다.
	StartListeningForParrySuccess();

	// 가드 진입 모션(= 패링 시도 몽타주, ANS_ParryWindow 호스트). 무기 DA의 Parry.ParryMontage에서 로드한다.
	// 지속 블록 포즈는 ALS 오버레이(State.Player.Guarding → IsGuarding())가 담당하므로 몽타주가 없어도 가드는 성립한다.
	if (const FWeaponParryData* ParryData = ResolveParryData())
	{
		if (UAnimMontage* Montage = ParryData->ParryMontage.LoadSynchronous())
		{
			MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
				this, NAME_None, Montage, ParryData->ParryMontagePlayRate, NAME_None, /*bStopWhenAbilityEnds=*/true);
			if (MontageTask)
			{
				MontageTask->ReadyForActivation();
			}
		}
	}

	InputReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, /*bTestAlreadyReleased=*/false);
	if (InputReleaseTask)
	{
		InputReleaseTask->OnRelease.AddDynamic(this, &ThisClass::HandleInputReleased);
		InputReleaseTask->ReadyForActivation();
	}

	GuardBrokenTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, RetrieveGameplayTags::GameplayEvent_Guard_Broken, nullptr, /*OnlyTriggerOnce=*/true, /*OnlyMatchExact=*/true);
	if (GuardBrokenTask)
	{
		GuardBrokenTask->EventReceived.AddDynamic(this, &ThisClass::HandleGuardBroken);
		GuardBrokenTask->ReadyForActivation();
	}
	
	// 가드 지속 비용: 점검 타이머가 DT_StaminaCost의 DrainPerSecond를 매 틱 소모하고, 소진 시 종료(권한 전용).
	if (HasAuthority(&ActivationInfo))
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				GuardStaminaTimerHandle,
				this,
				&ThisClass::HandleGuardStaminaTick,
				FMath::Max(StaminaCostTickInterval, 0.01f),
				true);
		}
	}
}

void UGA_Guard::HandleInputReleased(float /*TimeHeld*/)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}

void UGA_Guard::HandleGuardBroken(FGameplayEventData /*Payload*/)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (IsValid(ASC) && GuardBreakStaggerEffect && HasAuthority(&GetCurrentActivationInfoRef()))
	{
		const FGameplayEffectSpecHandle Spec = MakeSourcedSpec(GuardBreakStaggerEffect, 1.f);
		if (Spec.IsValid())
		{
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
}

void UGA_Guard::HandleGuardStaminaTick()
{
	// 실제 교전(IsCombatActive) 중에만 드레인 — 비전투 가드는 무료(질주와 동일 규칙).
	const UWorld* World = GetWorld();
	const URetrieveMusicSubsystem* MusicSubsystem = World ? World->GetSubsystem<URetrieveMusicSubsystem>() : nullptr;
	if (!MusicSubsystem || !MusicSubsystem->IsCombatActive())
	{
		return;
	}

	const float Interval = FMath::Max(StaminaCostTickInterval, 0.01f);

	FStaminaCostRow Row;
	const float DrainPerSecond = GetStaminaCostRow(Row) ? Row.DrainPerSecond : 0.f;
	if (DrainPerSecond > 0.f)
	{
		ApplyStaminaDelta(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, -DrainPerSecond * Interval);
	}

	if (!HasStamina(CurrentActorInfo, KINDA_SMALL_NUMBER))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
	}
}

bool UGA_Guard::OpenNotifyParryWindow()
{
	if (!WeaponCanParry())
	{
		return false;
	}
	return OpenParryWindow();
}

void UGA_Guard::CloseNotifyParryWindow()
{
	CloseParryWindow();
}

void UGA_Guard::StopRuntimeTasks()
{
	if (MontageTask)      { MontageTask->EndTask();      MontageTask = nullptr; }
	if (InputReleaseTask) { InputReleaseTask->EndTask(); InputReleaseTask = nullptr; }
	if (GuardBrokenTask)  { GuardBrokenTask->EndTask();  GuardBrokenTask = nullptr; }

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GuardStaminaTimerHandle);
	}
}

void UGA_Guard::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	StopRuntimeTasks();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
