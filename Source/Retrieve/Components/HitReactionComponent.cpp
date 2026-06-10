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
	// TODO(하민): 임시 재진입 차단 -> 식별 태그 없는 공격 건너뛰기로 대체 예정
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

	// 더 강한 상태가 이미 active면 약한 반응 무시
	if (!Entry->BlockingStateTags.IsEmpty())
	{
		if (ASC->HasAnyMatchingGameplayTags(Entry->BlockingStateTags))
		{
			return;
		}
	}

	if (Entry->StateEffect || Entry->bCancelActions)
	{
		StopActiveMontage();
		ApplyStateEffect(Entry->StateEffect);
		if (Entry->bCancelActions)
		{
			CancelOwnerAbilities();
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

void UHitReactionComponent::CancelOwnerAbilities()
{
	URetrieveAbilitySystemComponent* ASC = GetASC();
	if (!ASC || !Profile || Profile->AbilitiesToCancel.IsEmpty())
	{
		return;
	}

	FGameplayTagContainer CancelTags = Profile->AbilitiesToCancel;
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
