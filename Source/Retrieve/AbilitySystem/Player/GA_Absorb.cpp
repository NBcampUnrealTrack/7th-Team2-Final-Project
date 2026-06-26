#include "AbilitySystem/Player/GA_Absorb.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Components/Element/ElementGaugeComponent.h"
#include "Components/Player/WeaponComponent.h"
#include "Data/WeaponAttackDefinition.h"
#include "GameplayEffect.h"
#include "GameplayTags/RetrieveGameplayTags.h"
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
}

UGA_Absorb::UGA_Absorb()
{
	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_Absorb);
	SetAssetTags(Tags);
	
	ActivationOwnedTags.AddTag(RetrieveGameplayTags::Animation_Lock_Movement);
	ActivationOwnedTags.AddTag(RetrieveGameplayTags::Animation_Lock_Rotation);
}

void UGA_Absorb::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AActor* Avatar = ActorInfo->AvatarActor.Get();
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
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

	TSubclassOf<UGameplayEffect> EffectClass = nullptr;
	if (const TSubclassOf<UGameplayEffect>* EffectClassPtr = ElementToAbsorbEffect.Find(TargetElement))
	{
		EffectClass = *EffectClassPtr;
	}
	if (!EffectClass)
	{
		EffectClass = LoadDefaultAbsorbEffect(TargetElement);
	}
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
		this, NAME_None, Montage, FMath::Max(0.1f, PlayRate), NAME_None, /*bStopWhenAbilityEnds=*/true);
	if (!MontageTask)
	{
		return false;
	}

	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleCastMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleCastMontageFinished);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleCastMontageFinished);
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

void UGA_Absorb::HandleCastMontageFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}

void UGA_Absorb::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
