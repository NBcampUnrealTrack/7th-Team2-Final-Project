

#include "GA_Queen_Lifesteal.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Components/Enemy/BossPhaseComponent.h"
#include "GameFramework/Pawn.h"
#include "GameplayTags/RetrieveGameplayTags.h"

UGA_Queen_Lifesteal::UGA_Queen_Lifesteal(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ActivationPolicy = ERetrieveAbilityActivationPolicy::OnSpawn;
}

void UGA_Queen_Lifesteal::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	// 패시브: 여왕이 가한 HitSuccess(자식 태그 포함)를 계속 구독
	WaitHitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		RetrieveGameplayTags::GameplayEvent_Attack_HitSuccess,
		nullptr,
		/*OnlyTriggerOnce*/ false,
		/*OnlyMatchExact*/ false);   // Light/Heavy/WeakPoint 등 자식 전부 매칭

	if (IsValid(WaitHitTask))
	{
		WaitHitTask->EventReceived.AddDynamic(this, &UGA_Queen_Lifesteal::OnQueenDealtDamage);
		WaitHitTask->ReadyForActivation();
	}
	// 패시브라 EndAbility 호출 안 함 (계속 대기)
}

void UGA_Queen_Lifesteal::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (IsValid(WaitHitTask))
	{
		WaitHitTask->EndTask();
		WaitHitTask = nullptr;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_Queen_Lifesteal::OnQueenDealtDamage(FGameplayEventData Payload)
{
	const APawn* Queen = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (IsValid(Queen) == false)
	{
		return;
	}

	// 페이즈 게이트
	const UBossPhaseComponent* Phase = Queen->FindComponentByClass<UBossPhaseComponent>();
	if (Phase == nullptr || Phase->GetCurrentPhase() < MinPhase)
	{
		return;
	}

	const float Heal = Payload.EventMagnitude * LifestealRatio;   // 가한 데미지 × 비율
	if (Heal <= 0.f)
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (IsValid(ASC) == false || LifestealHealEffect == nullptr)
	{
		return;
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(LifestealHealEffect, 1.f, Context);
	if (Spec.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Heal_Magnitude, Heal);
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}
