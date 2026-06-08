#include "Components/HitReactionComponent.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameplayEffect.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Combat/RetrieveHitReactionProfile.h"
#include "Components/RetrievePawnExtensionComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"


UHitReactionComponent::UHitReactionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHitReactionComponent::BeginPlay()
{
	Super::BeginPlay();

	if (URetrievePawnExtensionComponent* PawnExt =
		URetrievePawnExtensionComponent::FindPawnExtensionComponent(GetOwner()))
	{
		PawnExt->OnAbilitySystemInitialized_RegisterAndCall(
			FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &UHitReactionComponent::OnAbilitySystemReady));
	}
}

void UHitReactionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HitEventHandle.IsValid())
	{
		if (URetrieveAbilitySystemComponent* ASC = GetASC())
		{
			FGameplayTagContainer Filter;
			Filter.AddTag(RetrieveGameplayTags::GameplayEvent_Hit);
			ASC->RemoveGameplayEventTagContainerDelegate(Filter, HitEventHandle);
		}
		HitEventHandle.Reset();
	}

	StopActiveMontage();
	Super::EndPlay(EndPlayReason);
}

void UHitReactionComponent::Configure(URetrieveHitReactionProfile* InProfile)
{
	Profile = InProfile;
}

URetrieveAbilitySystemComponent* UHitReactionComponent::GetASC() const
{
	URetrievePawnExtensionComponent* PawnExt =
		URetrievePawnExtensionComponent::FindPawnExtensionComponent(GetOwner());
	return PawnExt ? PawnExt->GetRetrieveAbilitySystemComponent() : nullptr;
}

void UHitReactionComponent::OnAbilitySystemReady()
{
	URetrieveAbilitySystemComponent* ASC = GetASC();
	if (!ASC || HitEventHandle.IsValid())
	{
		return;
	}

	FGameplayTagContainer Filter;
	Filter.AddTag(RetrieveGameplayTags::GameplayEvent_Hit);
	HitEventHandle = ASC->AddGameplayEventTagContainerDelegate(
		Filter,
		FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(this, &UHitReactionComponent::HandleHitEvent));
}

void UHitReactionComponent::HandleHitEvent(FGameplayTag /*MatchingTag*/, const FGameplayEventData* Payload)
{
	// TODO(하민): 임시 재진입 차단 -> 추후 식별 태그 없는 공격은 건너뛰기 구현 예정
	if (bProcessingReaction)
	{
		return;
	}

	bProcessingReaction = true;
	ApplyReaction(ResolveReactType(Payload));
	bProcessingReaction = false;
}

ERetrieveHitReactType UHitReactionComponent::ResolveReactType(const FGameplayEventData* Payload)
{
	if (!Payload)
	{
		return ERetrieveHitReactType::Flinch;
	}

	const FGameplayTagContainer& Tags = Payload->TargetTags;
	if (Tags.HasTagExact(RetrieveGameplayTags::HitReact_Type_Knockdown)) { return ERetrieveHitReactType::Knockdown; }
	if (Tags.HasTagExact(RetrieveGameplayTags::HitReact_Type_Stagger)) { return ERetrieveHitReactType::Stagger; }

	return ERetrieveHitReactType::Flinch;
}

void UHitReactionComponent::ApplyReaction(ERetrieveHitReactType ReactType)
{
	URetrieveAbilitySystemComponent* ASC = GetASC();
	if (!ASC || !Profile)
	{
		return;
	}
	
	const FRetrieveHitReactionEntry* Entry = Profile->Find(ReactType);
	if (!Entry)
	{
		return;
	}

	const bool bKnockdownActive = ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Knockdown);
	const bool bStaggerActive = ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Staggered);

	// 더 강한 상태가 Active면 약한 반응은 무시 (Knockdown > Stagger > Flinch)
	switch (ReactType)
	{
	case ERetrieveHitReactType::Knockdown:
		break;

	case ERetrieveHitReactType::Stagger:
		if (bKnockdownActive)
		{
			return;
		}
		break;

	case ERetrieveHitReactType::Flinch:
	default:
		if (bKnockdownActive || bStaggerActive)
		{
			return;
		}
		break;
	}

	// 상태/취소가 있는 반응은 이전 몽타주를 끊고 다시 재생
	if (Entry->StateEffect || Entry->bCancelActions)
	{
		StopActiveMontage();
		ApplyStateEffect(Entry->StateEffect);
		if (Entry->bCancelActions)
		{
			CancelPlayerActions();
		}
	}

	PlayMontageSafe(Entry->Montage);
}

void UHitReactionComponent::ApplyStateEffect(const TSubclassOf<UGameplayEffect>& EffectClass)
{
	if (!EffectClass)
	{
		return;
	}

	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	URetrieveAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
	{
		return;
	}

	FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
	Ctx.AddSourceObject(this);
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, 1.f, Ctx);
	if (Spec.IsValid())
	{
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

void UHitReactionComponent::CancelPlayerActions()
{
	URetrieveAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
	{
		return;
	}

	FGameplayTagContainer CancelTags;
	CancelTags.AddTag(RetrieveGameplayTags::Ability_Player_Attack);
	CancelTags.AddTag(RetrieveGameplayTags::Ability_Player_HeavyAttack);
	CancelTags.AddTag(RetrieveGameplayTags::Ability_Player_Guard);
	CancelTags.AddTag(RetrieveGameplayTags::Ability_Player_Dash);
	ASC->CancelAbilities(&CancelTags, nullptr, nullptr);
}

void UHitReactionComponent::PlayMontageSafe(const TSoftObjectPtr<UAnimMontage>& MontagePtr)
{
	UAnimMontage* Montage = MontagePtr.LoadSynchronous();
	if (!Montage)
	{
		return;
	}

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return;
	}

	AnimInstance->Montage_Play(Montage);
	ActiveMontage = Montage;
}

void UHitReactionComponent::StopActiveMontage()
{
	if (!ActiveMontage)
	{
		return;
	}

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr)
	{
		AnimInstance->Montage_Stop(0.1f, ActiveMontage);
	}
	ActiveMontage = nullptr;
}
