#include "AbilitySystem/Player/GA_ParryBase.h"

#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/Element/ElementGaugeComponent.h"
#include "Components/Player/CounterTimeDilationComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameplayEffect.h"
#include "UObject/SoftObjectPath.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"

bool UGA_ParryBase::OpenParryWindow()
{
	// Open은 패링 판정 태그(State.Player.Parrying)를 부여하는 책임만 가진다.
	// 쿨다운까지 여기서 걸면 NotifyState 기반 GuardAttack에서 window 구간과 cooldown 구간이 섞인다.
	if (bParryWindowOpened)
	{
		return false;
	}
	
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(ASC) || !ParryWindowEffect)
	{
		return false;
	}
	
	if (ASC->HasMatchingGameplayTag(RetrieveGameplayTags::Cooldown_Player_Parry))
	{
		// 쿨다운 중에는 window 자체를 열지 않는다. GuardAttack은 이 상태에서 발동도 차단할 예정이다.
		return false;
	}
	
	FGameplayEffectSpecHandle SpecHandle = MakeSourcedSpec(ParryWindowEffect, GetAbilityLevel());
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return false;
	}
	
	ParryWindowHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	if (!ParryWindowHandle.IsValid())
	{
		return false;	
	}
	
	bParryWindowOpened = true;
	return true;
}
void UGA_ParryBase::CloseParryWindow()
{
	// 성공, NotifyState End, EndAbility가 모두 닫기를 시도할 수 있으므로 idempotent하게 둔다.
	if (!bParryWindowOpened && !ParryWindowHandle.IsValid())
	{
		return;
	}
	
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (ParryWindowHandle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(ParryWindowHandle);
		}
	}
	
	ParryWindowHandle.Invalidate();
	bParryWindowOpened = false;
}

void UGA_ParryBase::ApplyParryCooldown()
{
	// Cooldown은 패링 시도자 자신에게만 적용된다.
	// Guard 자체나 ParryCounter를 막는 목적이 아니라 다음 ParryWindow 오픈만 제한하는 목적이다.
	if (bParryCooldownApplied || !ParryCooldownEffect)
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(ASC))
	{
		return;
	}

	if (ASC->HasMatchingGameplayTag(RetrieveGameplayTags::Cooldown_Player_Parry))
	{
		bParryCooldownApplied = true;
		return;
	}

	FGameplayEffectSpecHandle SpecHandle = MakeSourcedSpec(ParryCooldownEffect, GetAbilityLevel());
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return;
	}

	ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	bParryCooldownApplied = true;
}

void UGA_ParryBase::StartListeningForParrySuccess()
{
	StopParrySuccessTask();

	ParrySuccessTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, RetrieveGameplayTags::GameplayEvent_Parry_Success, nullptr, /*OnlyTriggerOnce=*/false, /*OnlyMatchExact=*/true);
	if (ParrySuccessTask)
	{
		ParrySuccessTask->EventReceived.AddDynamic(this, &ThisClass::HandleParrySuccess);
		ParrySuccessTask->ReadyForActivation();
	}
}

void UGA_ParryBase::StopParrySuccessTask()
{
	if (ParrySuccessTask)
	{
		ParrySuccessTask->EndTask();
		ParrySuccessTask = nullptr;
	}
}

void UGA_ParryBase::HandleParrySuccess(FGameplayEventData Payload)
{
	bParrySucceeded = true;

	// 성공 즉시 window를 닫아 같은 NotifyState/GE 구간에서 다중 패링이 연쇄 발생하지 않게 한다.
	// 이후 CounterWindow는 별도로 열리므로 "패링 성공 보상"은 유지된다.
	CloseParryWindow();
	ApplyParryCooldown();
	StopParrySuccessTask();
	
	LastParriedAttacker = const_cast<AActor*>(Cast<AActor>(Payload.OptionalObject));
	if (URetrieveAbilitySystemComponent* RetrieveASC = Cast<URetrieveAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo()))
	{
		RetrieveASC->SetPendingCounterTarget(LastParriedAttacker.Get());
	}
	
	// 패리 성공 시 스태미너 회복(스태미너 소모 설정 맵의 RestoreAmount, 이 어빌리티 StaminaCostTag 항목).
	FStaminaCostRow StaminaRow;
	if (GetStaminaCostRow(StaminaRow) && StaminaRow.RestoreAmount > 0.f)
	{
		ApplyStaminaDelta(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, StaminaRow.RestoreAmount);
	}

	// 패링 성공 어드밴티지: 짧은 공격 모멘텀 버프 + 원소 게이지 충전 (잘 막으면 보상)
	{
		TSubclassOf<UGameplayEffect> MomentumEffect = ParryMomentumEffect;
		if (!MomentumEffect)
		{
			MomentumEffect = FSoftClassPath(TEXT("/Game/Retrieve/AbilitySystem/Player/Advantage/GE_ParryMomentum.GE_ParryMomentum_C")).TryLoadClass<UGameplayEffect>();
		}
		if (MomentumEffect)
		{
			ApplyGameplayEffectToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, MomentumEffect.GetDefaultObject(), GetAbilityLevel());
		}

		if (ParryElementGaugeCharge > 0)
		{
			if (AActor* Avatar = GetAvatarActorFromActorInfo())
			{
				if (UElementGaugeComponent* Gauge = Avatar->FindComponentByClass<UElementGaugeComponent>())
				{
					Gauge->AddCharge(ParryElementGaugeCharge);
				}
			}
		}
	}

	// 성공 피드백만 즉시 실행한다.
	// 카운터 대시는 GA_ParryCounter가 Attack 입력으로 발동된 뒤 ParrySuccessMontage를 재생한다.
	ExecuteParrySuccessCue();

	// 카운터 선택 대기 연출 시작.
	// 입력이 없으면 CounterTimeDilationComponent가 만료 시 원복하고,
	// 입력이 있으면 카운터 ability가 EnterReboost()로 대시 전환을 시작한다.
	if (AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		if (UCounterTimeDilationComponent* TimeComp = Avatar->FindComponentByClass<UCounterTimeDilationComponent>())
		{
			TimeComp->StartWindowSlow();
		}
	}

	// 적 반응 확정: 비보스=ParryStaggerEffect, 보스=BossParryStaggerEffect(약화)
	ApplyParryStagger(LastParriedAttacker.Get());
	
	if (AActor* CounterAvatar = GetAvatarActorFromActorInfo())
	{
		FGameplayEventData CounterEvent;
		CounterEvent.EventTag = RetrieveGameplayTags::GameplayEvent_Parry_Counter;
		CounterEvent.Instigator = CounterAvatar;
		CounterEvent.Target = LastParriedAttacker.Get();
		CounterEvent.OptionalObject = LastParriedAttacker.Get();
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(CounterAvatar, RetrieveGameplayTags::GameplayEvent_Parry_Counter, CounterEvent);
	}
}

void UGA_ParryBase::ExecuteParrySuccessCue() const
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		FGameplayCueParameters Params;
		Params.Instigator = GetAvatarActorFromActorInfo();
		Params.EffectCauser = LastParriedAttacker.Get();
		ASC->ExecuteGameplayCue(RetrieveGameplayTags::GameplayCue_Parry_Success, Params);
	}
}

void UGA_ParryBase::ApplyParryStagger(AActor* Attacker)
{
	if (!HasAuthority(&GetCurrentActivationInfoRef()) || !IsValid(Attacker))
	{
		return;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Attacker);
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!TargetASC || !SourceASC)
	{
		return;
	}

	const TSubclassOf<UGameplayEffect> StaggerGE = SelectEffectByTargetType(TargetASC, ParryStaggerEffect, BossParryStaggerEffect);
	const FGameplayEffectSpecHandle Spec = MakeSourcedSpec(StaggerGE, GetAbilityLevel());
	if (Spec.IsValid())
	{
		SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	}
}

void UGA_ParryBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 실패/만료/취소 경로에서도 window가 열린 적이 있으면 cooldown을 적용한다.
	// CloseParryWindow()가 상태를 false로 만들기 때문에 먼저 스냅샷을 잡는다.
	const bool bHadParryWindow = bParryWindowOpened || ParryWindowHandle.IsValid();
	
	CloseParryWindow();
	
	if (bHadParryWindow && !bParryCooldownApplied)
	{
		ApplyParryCooldown();
	}
	
	StopParrySuccessTask();
	
	bParrySucceeded = false;
	bParryCooldownApplied = false;
	
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
