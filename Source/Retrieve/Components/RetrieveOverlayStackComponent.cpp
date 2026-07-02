#include "Components/RetrieveOverlayStackComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Components/Pawn/RetrievePawnExtensionComponent.h"
#include "Components/MeshComponent.h"
#include "Data/RetrieveOverlayMaterialMapDataAsset.h"
#include "Materials/MaterialInterface.h"

URetrieveOverlayStackComponent::URetrieveOverlayStackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URetrieveOverlayStackComponent::BeginPlay()
{
	Super::BeginPlay();

	URetrievePawnExtensionComponent* PawnExt =
		URetrievePawnExtensionComponent::FindPawnExtensionComponent(GetOwner());
	if (!PawnExt)
	{
		return;
	}

	FSimpleMulticastDelegate::FDelegate Callback =
		FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &URetrieveOverlayStackComponent::OnAbilitySystemReady);
	PawnExt->OnAbilitySystemInitialized_RegisterAndCall(Callback);
}

void URetrieveOverlayStackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (URetrieveAbilitySystemComponent* ASC = BoundASC.Get())
	{
		for (const TPair<FGameplayTag, FDelegateHandle>& Binding : TagBindings)
		{
			ASC->UnregisterGameplayTagEvent(Binding.Value, Binding.Key, EGameplayTagEventType::NewOrRemoved);
		}
	}
	TagBindings.Reset();
	BoundASC.Reset();

	if (CurrentMaterial.IsValid())
	{
		ApplyOverlayToOwnerMeshes(nullptr);
	}
	CurrentMaterial.Reset();

	Super::EndPlay(EndPlayReason);
}

void URetrieveOverlayStackComponent::OnAbilitySystemReady()
{
	URetrievePawnExtensionComponent* PawnExt =
		URetrievePawnExtensionComponent::FindPawnExtensionComponent(GetOwner());
	if (!PawnExt)
	{
		return;
	}

	URetrieveAbilitySystemComponent* ASC = PawnExt->GetRetrieveAbilitySystemComponent();
	if (!ASC || !OverlayMap)
	{
		return;
	}

	BoundASC = ASC;

	FGameplayTagContainer KeyTags;
	OverlayMap->GetKeyTags(KeyTags);

	for (const FGameplayTag& Tag : KeyTags)
	{
		FDelegateHandle Handle = ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &URetrieveOverlayStackComponent::OnTrackedTagChanged);
		TagBindings.Emplace(Tag, Handle);
	}

	Recalculate();
}

void URetrieveOverlayStackComponent::OnTrackedTagChanged(const FGameplayTag /*Tag*/, int32 /*NewCount*/)
{
	Recalculate();
}

void URetrieveOverlayStackComponent::Recalculate()
{
	URetrieveAbilitySystemComponent* ASC = BoundASC.Get();
	if (!ASC || !OverlayMap)
	{
		return;
	}

	const FRetrieveOverlayEntry* TopEntry = nullptr;
	FGameplayTag TopTag;

	for (const TPair<FGameplayTag, FDelegateHandle>& Binding : TagBindings)
	{
		const FGameplayTag& Tag = Binding.Key;
		if (ASC->GetTagCount(Tag) <= 0)
		{
			continue;
		}

		const FRetrieveOverlayEntry* Entry = OverlayMap->Find(Tag);
		if (!Entry)
		{
			continue;
		}

		if (!TopEntry)
		{
			TopEntry = Entry;
			TopTag = Tag;
			continue;
		}

		// Priority DESC, 동률 시 TagName ASC(사전순)
		const bool bHigherPriority = Entry->Priority > TopEntry->Priority;
		const bool bTiedAndEarlierName =
			Entry->Priority == TopEntry->Priority && Tag.ToString() < TopTag.ToString();
		if (bHigherPriority || bTiedAndEarlierName)
		{
			TopEntry = Entry;
			TopTag = Tag;
		}
	}

	UMaterialInterface* DesiredMaterial = TopEntry ? TopEntry->Material.Get() : nullptr;
	if (CurrentMaterial.Get() == DesiredMaterial)
	{
		return;
	}

	ApplyOverlayToOwnerMeshes(DesiredMaterial);
	CurrentMaterial = DesiredMaterial;
}

void URetrieveOverlayStackComponent::ApplyOverlayToOwnerMeshes(UMaterialInterface* Material) const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TArray<UMeshComponent*> MeshComps;
	Owner->GetComponents<UMeshComponent>(MeshComps);
	for (UMeshComponent* Mesh : MeshComps)
	{
		if (Mesh)
		{
			Mesh->SetOverlayMaterial(Material);
		}
	}
}