#include "AbilitySystem/Player/GA_Absorb.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Components/Element/ElementGaugeComponent.h"
#include "Components/Element/ElementResonanceComponent.h"
#include "Components/Player/WeaponComponent.h"
#include "Data/WeaponAttackDefinition.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Player/RetrievePlayerState.h"
#include "UI/HUD/RetrieveBuffUIBroadcastComponent.h"
#include "UI/RetrieveElementUILibrary.h"
#include "UObject/SoftObjectPath.h"

namespace
{
FGameplayTag ResolveAbsorbBuffUITag(FGameplayTag ElementTag, const TMap<FGameplayTag, FGameplayTag>& OverrideMap)
{
	if (const FGameplayTag* BuffUITag = OverrideMap.Find(ElementTag))
	{
		return *BuffUITag;
	}

	return URetrieveElementUILibrary::ElementToAbsorbBuffUITag(ElementTag);
}

// Legacy fallback: BP_GA_Absorb의 ElementToAbsorbEffect가 비어 있던 기존 자산을 위한 C++ 고정 경로.
// 신규 데이터는 어빌리티 기본값/데이터 에셋에서 명시적으로 지정하는 쪽을 우선한다.
TSubclassOf<UGameplayEffect> LoadDefaultAbsorbEffect(FGameplayTag ElementTag)
{
	const TCHAR* EffectPath = nullptr;
	if (ElementTag.MatchesTagExact(RetrieveGameplayTags::Element_Fire))
	{
		EffectPath = TEXT("/Game/Retrieve/AbilitySystem/Player/GE_Absorb_Fire.GE_Absorb_Fire_C");
	}
	else if (ElementTag.MatchesTagExact(RetrieveGameplayTags::Element_Water))
	{
		EffectPath = TEXT("/Game/Retrieve/AbilitySystem/Player/GE_Absorb_Water.GE_Absorb_Water_C");
	}
	else if (ElementTag.MatchesTagExact(RetrieveGameplayTags::Element_Wind))
	{
		EffectPath = TEXT("/Game/Retrieve/AbilitySystem/Player/GE_Absorb_Wind.GE_Absorb_Wind_C");
	}

	return EffectPath ? FSoftClassPath(EffectPath).TryLoadClass<UGameplayEffect>() : nullptr;
}

// 어빌리티 기본값에 어튠 스택 GE가 비어 있을 때 사용하는 고정 경로 폴백 (BP 재저장 없이 동작)
TSubclassOf<UGameplayEffect> LoadDefaultAttuneStackEffect(FGameplayTag ElementTag)
{
	const TCHAR* EffectPath = nullptr;
	if (ElementTag.MatchesTagExact(RetrieveGameplayTags::Element_Fire))
	{
		EffectPath = TEXT("/Game/Retrieve/AbilitySystem/Player/Resonance/GE_ElementStack_Fire.GE_ElementStack_Fire_C");
	}
	else if (ElementTag.MatchesTagExact(RetrieveGameplayTags::Element_Water))
	{
		EffectPath = TEXT("/Game/Retrieve/AbilitySystem/Player/Resonance/GE_ElementStack_Water.GE_ElementStack_Water_C");
	}
	else if (ElementTag.MatchesTagExact(RetrieveGameplayTags::Element_Wind))
	{
		EffectPath = TEXT("/Game/Retrieve/AbilitySystem/Player/Resonance/GE_ElementStack_Wind.GE_ElementStack_Wind_C");
	}

	return EffectPath ? FSoftClassPath(EffectPath).TryLoadClass<UGameplayEffect>() : nullptr;
}
}

UGA_Absorb::UGA_Absorb()
{
	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_Absorb);
	Tags.AddTag(RetrieveGameplayTags::Ability_Type_Attack);
	SetAssetTags(Tags);

	// 평타 콤보 도중 캔슬 윈도우가 열린 구간에서 흡수로 전환할 수 있게 버퍼를 탄다.
	// 흡수는 원소 상태 전환이 아니라 공격형 전투 액션이다.
	// Ability.Type.Attack을 붙여 활성 중 후속 입력도 AttackCancelWindow 정책을 따르게 한다.
	bUseCombatInputBuffer = true;
	CombatInputPriority   = 10;

	bBlockActivationWhileAirborne = true;
	bBlockedByLocomotionAction = true;

	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dead);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Knockdown);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dodging);

	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Player_Attacking);

	CancelAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Type_Attack);
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Guard);
}

TSubclassOf<UGameplayEffect> UGA_Absorb::ResolveAbsorbEffectClass(const FGameplayTag Element) const
{
	if (const TSubclassOf<UGameplayEffect>* EffectClassPtr = ElementToAbsorbEffect.Find(Element))
	{
		return *EffectClassPtr;
	}

	return LoadDefaultAbsorbEffect(Element);
}

TSubclassOf<UGameplayEffect> UGA_Absorb::ResolveAttuneStackEffectClass(const FGameplayTag Element) const
{
	if (const TSubclassOf<UGameplayEffect>* EffectClassPtr = ElementToAttuneStackEffect.Find(Element))
	{
		return *EffectClassPtr;
	}

	return LoadDefaultAttuneStackEffect(Element);
}

bool UGA_Absorb::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!IsValid(Avatar) || !IsValid(ASC))
	{
		return false;
	}

	// CanActivate에서는 소비하지 않는다. 충전 슬롯이 없으면 TryActivateAbility 자체를 실패시켜
	// CancelAbilitiesWithTag가 기존 평타를 끊지 못하게 한다.
	const UElementGaugeComponent* Gauge = Avatar->FindComponentByClass<UElementGaugeComponent>();
	if (!IsValid(Gauge) || !Gauge->HasChargedSlot())
	{
		return false;
	}

	// CanActivateAbility는 CDO 경로로 호출될 가능성을 배제하지 않고 ActorInfo에서 직접 원소를 읽는다.
	// ResolveCurrentElementTag()는 활성 인스턴스의 ActorInfo에 기대므로 ActivateAbility에서만 사용한다.
	const APawn* AvatarPawn = Cast<APawn>(Avatar);
	const ARetrievePlayerState* RetrievePlayerState = AvatarPawn ? AvatarPawn->GetPlayerState<ARetrievePlayerState>() : nullptr;
	const FGameplayTag TargetElement = RetrievePlayerState ? RetrievePlayerState->GetCurrentElementTag() : FGameplayTag();
	if (!TargetElement.IsValid() || TargetElement.MatchesTagExact(RetrieveGameplayTags::Element_None))
	{
		return false;
	}

	const TSubclassOf<UGameplayEffect> EffectClass = ResolveAbsorbEffectClass(TargetElement);
	return EffectClass != nullptr;
}

void UGA_Absorb::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!IsValid(Avatar) || !IsValid(ASC))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UElementGaugeComponent* Gauge = Avatar->FindComponentByClass<UElementGaugeComponent>();
	if (!IsValid(Gauge))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 흡수 원소는 게이지 슬롯이 아니라 현재 선택된 원소모드로 결정한다.
	const FGameplayTag TargetElement = ResolveCurrentElementTag();
	if (!TargetElement.IsValid() || TargetElement.MatchesTagExact(RetrieveGameplayTags::Element_None))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	TSubclassOf<UGameplayEffect> EffectClass = ResolveAbsorbEffectClass(TargetElement);
	if (!EffectClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 원소와 무관하게 충전된 슬롯 한 칸을 소비한다. 소비할 슬롯이 없으면 종료.
	if (!Gauge->ConsumeOldestSlot())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);

	const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(EffectClass, GetAbilityLevel(), Context);
	if (SpecHandle.IsValid())
	{
		const FActiveGameplayEffectHandle AppliedHandle =
			ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);

		const FGameplayTag BuffUITag = ResolveAbsorbBuffUITag(TargetElement, ElementToAbsorbBuffUITag);
		if (BuffUITag.IsValid())
		{
			if (URetrieveBuffUIBroadcastComponent* BuffUI =
				Avatar->FindComponentByClass<URetrieveBuffUIBroadcastComponent>())
			{
				const float GEDuration = SpecHandle.Data->GetDuration();
				// GE가 보고한 실제 스택 수를 UI에 그대로 전달 → StackLimitCount cap이 그대로 반영된다.
				const int32 StackCount = ASC->GetCurrentStackCount(AppliedHandle);
				BuffUI->BroadcastBuffManual(BuffUITag, GEDuration > 0.f ? GEDuration : 0.f, EffectClass, StackCount);
			}
		}
	}

	// 원소 공명: 흡수한 원소의 어튠 스택 +1 (스택형 Duration GE. ElementResonanceComponent가 집계).
	// 단, 이미 공명 버프가 활성 중이면 어튠 스택을 쌓지 않는다(공명은 한 번에 하나만).
	const UElementResonanceComponent* ResonanceComp = Avatar->FindComponentByClass<UElementResonanceComponent>();
	const bool bResonanceActive = ResonanceComp && ResonanceComp->HasActiveResonance();
	if (!bResonanceActive)
	{
		if (const TSubclassOf<UGameplayEffect> StackEffectClass = ResolveAttuneStackEffectClass(TargetElement))
		{
			FGameplayEffectContextHandle StackContext = ASC->MakeEffectContext();
			StackContext.AddSourceObject(this);
			const FGameplayEffectSpecHandle StackSpecHandle = ASC->MakeOutgoingSpec(StackEffectClass, GetAbilityLevel(), StackContext);
			if (StackSpecHandle.IsValid())
			{
				ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, StackSpecHandle);
			}
		}
	}

	// 신규 표준: 흡수 시전 몽타주는 장착 무기의 AttackDefinition에서 먼저 찾는다.
	// GA_Absorb는 공용 AbilitySet에서 부여되므로 무기별 모션 데이터는 런타임 무기 데이터가 소유한다.
	const UWeaponComponent* WeaponComp = Avatar->FindComponentByClass<UWeaponComponent>();
	const UWeaponAttackDefinition* AttackDefinition = nullptr;
	if (IsValid(WeaponComp))
	{
		AttackDefinition = WeaponComp->GetWeaponDataRef().AttackComboDefinition.LoadSynchronous();
	}

	const FWeaponAbsorbCast* AbsorbCastData =
		AttackDefinition ? AttackDefinition->ResolveAbsorbVariant(TargetElement) : nullptr;
	if (AbsorbCastData && PlayCastMontage(AbsorbCastData->Montage, AbsorbCastData->MontagePlayRate))
	{
		return;
	}

	// 기존 BP_GA_Absorb 기본값을 바로 깨지 않기 위한 fallback.
	if (PlayLegacyCastMontage(TargetElement))
	{
		return;
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

bool UGA_Absorb::PlayCastMontage(const TSoftObjectPtr<UAnimMontage>& MontagePtr, const float PlayRate)
{
	UAnimMontage* Montage = MontagePtr.LoadSynchronous();
	if (!IsValid(Montage))
	{
		return false;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, Montage, FMath::Max(0.1f, PlayRate), NAME_None, /*bStopWhenAbilityEnds=*/true,
		1.f, 0.f, /*bAllowInterruptAfterBlendOut=*/true);
	if (!MontageTask)
	{
		return false;
	}

	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleCastMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleCastMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleCastMontageCancelled);
	MontageTask->ReadyForActivation();
	return true;
}

bool UGA_Absorb::PlayLegacyCastMontage(const FGameplayTag Element)
{
	const TSoftObjectPtr<UAnimMontage>* MontagePtr = ElementToCastMontage.Find(Element);
	if (!MontagePtr)
	{
		return false;
	}

	return PlayCastMontage(*MontagePtr, CastMontagePlayRate);
}

void UGA_Absorb::HandleCastMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}

void UGA_Absorb::HandleCastMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
}

void UGA_Absorb::HandleCastMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
}

void UGA_Absorb::CleanupAbsorb()
{
	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}
}

void UGA_Absorb::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	CleanupAbsorb();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_Absorb::CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility)
{
	CleanupAbsorb();
	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}
