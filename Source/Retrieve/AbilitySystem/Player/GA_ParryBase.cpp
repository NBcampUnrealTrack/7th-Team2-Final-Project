#include "AbilitySystem/Player/GA_ParryBase.h"

#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Components/Element/ElementGaugeComponent.h"
#include "Components/Player/WeaponComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Data/WeaponAttackDefinition.h"
#include "GameFramework/Character.h"
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

	// 성공 즉시 window를 닫아 같은 구간에서 다중 패링이 연쇄되지 않게 한다.
	CloseParryWindow();
	StopParrySuccessTask();

	LastParriedAttacker = const_cast<AActor*>(Cast<AActor>(Payload.OptionalObject));

	ExecuteParrySuccessCue();

	// 카운터 자격(EventMagnitude): 1=일반몹/보스 카운터패턴, 0=보스 단순 막기.
	// 자격 없으면 데미지만 막고(판정부에서 이미 0) 스태거·카운터·리액션 없이 종료.
	if (Payload.EventMagnitude <= 0.f)
	{
		return;
	}

	PlayParrySuccessMontage();

	// 카운터 대상 저장 + 수용 시간(무기 데이터). 만료 시 ASC가 대상 소멸. 카운터는 좌클릭 입력으로만 발동.
	if (URetrieveAbilitySystemComponent* RetrieveASC = Cast<URetrieveAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo()))
	{
		float CounterWindow = 0.f;
		if (const FWeaponParryData* ParryData = ResolveParryData())
		{
			CounterWindow = ParryData->CounterWindowDuration;
		}
		RetrieveASC->SetPendingCounterTarget(LastParriedAttacker.Get(), CounterWindow);
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

	// 타격+스태거는 즉시가 아니라 성공 몽타주의 AnimNotify_ParryImpact(방패 미는 프레임)가
	// PendingCounterTarget에게 적용한다. 여기선 대상만 확정(SetPendingCounterTarget)해 둔다.
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

void UGA_ParryBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	CloseParryWindow();
	StopParrySuccessTask();

	bParrySucceeded = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

const FWeaponParryData* UGA_ParryBase::ResolveParryData() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const UWeaponComponent* Weapon = Avatar ? Avatar->FindComponentByClass<UWeaponComponent>() : nullptr;
	if (!IsValid(Weapon) || !Weapon->IsEquipped())
	{
		return nullptr;
	}

	const UWeaponAttackDefinition* Def = Weapon->GetWeaponDataRef().AttackComboDefinition.LoadSynchronous();
	return Def ? &Def->Parry : nullptr;
}

bool UGA_ParryBase::WeaponCanParry() const
{
	const FWeaponParryData* ParryData = ResolveParryData();
	return ParryData && !ParryData->SuccessMontage.IsNull();
}

void UGA_ParryBase::PlayParrySuccessMontage() const
{
	const FWeaponParryData* ParryData = ResolveParryData();
	if (!ParryData || ParryData->SuccessMontage.IsNull())
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!IsValid(Character))
	{
		return;
	}

	if (UAnimMontage* Montage = ParryData->SuccessMontage.LoadSynchronous())
	{
		Character->PlayAnimMontage(Montage, ParryData->SuccessMontagePlayRate);
	}
}
