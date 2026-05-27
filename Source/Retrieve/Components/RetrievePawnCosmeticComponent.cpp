
#include "RetrievePawnCosmeticComponent.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "Character/Cosmetics/RetrieveCosmeticData.h"
#include "Components/WeaponComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Player/RetrievePlayerState.h"

URetrievePawnCosmeticComponent::URetrievePawnCosmeticComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URetrievePawnCosmeticComponent::InitializeWithAbilitySystem(UAbilitySystemComponent* ASC)
{
	if (!IsValid(ASC)) { return; }
	
	OwnerASC = ASC;

	if (CosmeticData)
	{
		CosmeticTags.AppendTags(CosmeticData->DefaultCosmeticTags);
	}

	const APawn* Pawn = GetPawn<APawn>();
	if (!IsValid(Pawn)) { return; }
	
	ARetrievePlayerState* PlayerState = Pawn->GetPlayerState<ARetrievePlayerState>();
	if (IsValid(PlayerState))
	{
		CurrentElementTag = PlayerState->GetCurrentElementTag();
		if (CurrentElementTag.IsValid())
		{
			CosmeticTags.AddTag(CurrentElementTag);
		}
	}

	ASC->GenericGameplayEventCallbacks
		.FindOrAdd(RetrieveGameplayTags::GameplayEvent_Element_ModeChange)
		.AddUObject(this, &ThisClass::OnElementModeChanged);
	
	AActor* Owner = GetOwner();
	if (!Owner) { return; }
	
	if (UWeaponComponent* WeaponComp = Owner->FindComponentByClass<UWeaponComponent>())
	{
		WeaponComp->OnWeaponEquipped.AddDynamic(this, &ThisClass::OnWeaponEquipped);
		WeaponComp->OnWeaponUnequipped.AddDynamic(this, &ThisClass::OnWeaponUnequipped);
	}
	ApplyCosmeticLayer();
}

void URetrievePawnCosmeticComponent::UninitializeFromAbilitySystem()
{
	if (IsValid(OwnerASC))
	{
		if (auto* Delegate = OwnerASC->GenericGameplayEventCallbacks.Find(RetrieveGameplayTags::GameplayEvent_Element_ModeChange))
		{
			Delegate->RemoveAll(this);
		}
		OwnerASC = nullptr;
	}
}

void URetrievePawnCosmeticComponent::OnWeaponEquipped(FName WeaponItemId)
{
	if (UWeaponComponent* WeaponComp = GetOwner()->FindComponentByClass<UWeaponComponent>())
	{
		CurrentWeaponTypeTag = WeaponComp->GetWeaponData().WeaponTypeTag;
		CosmeticTags.AddTag(CurrentWeaponTypeTag);
	}
	
	ApplyCosmeticLayer();
}

void URetrievePawnCosmeticComponent::OnWeaponUnequipped(FName WeaponItemId)
{
	CosmeticTags.RemoveTag(CurrentWeaponTypeTag);
	CurrentWeaponTypeTag = FGameplayTag::EmptyTag;
	
	ApplyCosmeticLayer();
}

void URetrievePawnCosmeticComponent::OnElementModeChanged(const FGameplayEventData* Payload)
{
	if (CurrentElementTag.IsValid())
	{
		CosmeticTags.RemoveTag(CurrentElementTag);
		CurrentElementTag = FGameplayTag::EmptyTag;
	}

	if (Payload && !Payload->InstigatorTags.IsEmpty())
	{
		TArray<FGameplayTag> Tags;
		Payload->InstigatorTags.GetGameplayTagArray(Tags);
		CurrentElementTag = Tags[0];
		CosmeticTags.AddTag(CurrentElementTag);
	}

	ApplyCosmeticLayer();
}

void URetrievePawnCosmeticComponent::ApplyCosmeticLayer()
{
	if (!IsValid(CosmeticData)) { return; }

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!IsValid(Character)) { return; }
	
	USkeletalMeshComponent* Mesh = Character->GetMesh();
	if (!IsValid(Mesh)) { return; }
	
	const FGameplayTagContainer ActiveTags = BuildCosmeticTags();
	const TSubclassOf<UAnimInstance> DesiredLayer = CosmeticData->AnimLayerRules.SelectBestLayer(ActiveTags);
	Mesh->LinkAnimClassLayers(DesiredLayer);
}

FGameplayTagContainer URetrievePawnCosmeticComponent::BuildCosmeticTags() const
{
	return CosmeticTags;
}
