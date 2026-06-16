#include "UI/Inventory/InventoryPreviewActor.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/Cosmetics/RetrieveModularMeshTypes.h"
#include "Components/Player/ArmorComponent.h"
#include "Components/Inventory/InventoryComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Engine/DataTable.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Kismet/GameplayStatics.h"

AInventoryPreviewActor::AInventoryPreviewActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AInventoryPreviewActor::BeginPlay()
{
	Super::BeginPlay();

	BindInventoryEvents();
	UpdateArmorPreview();
}

void AInventoryPreviewActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindInventoryEvents();
	ClearArmorMeshes();

	Super::EndPlay(EndPlayReason);
}

void AInventoryPreviewActor::ClearArmorMeshes()
{
	for (USkeletalMeshComponent* ArmorMeshComponent : ArmorMeshComponents)
	{
		if (IsValid(ArmorMeshComponent))
		{
			ArmorMeshComponent->DestroyComponent();
		}
	}

	ArmorMeshComponents.Reset();
}

void AInventoryPreviewActor::UpdateArmorPreview()
{
	ClearArmorMeshes();

	UInventoryComponent* InventoryComponent = BoundInventoryComponent.Get();
	if (!InventoryComponent)
	{
		InventoryComponent = ResolveInventoryComponent();
		BoundInventoryComponent = InventoryComponent;
	}

	UDataTable* ArmorDataTable = ResolveArmorDataTable();
	USkeletalMeshComponent* LeaderMeshComponent = ResolvePreviewMeshComponent();
	if (!InventoryComponent || !ArmorDataTable || !LeaderMeshComponent)
	{
		return;
	}

	const TArray<FRetrieveEquippedArmorEntry> EquippedArmorSlots = InventoryComponent->GetEquippedArmorSlots();
	for (const FRetrieveEquippedArmorEntry& EquippedArmor : EquippedArmorSlots)
	{
		if (EquippedArmor.ArmorItemId.IsNone())
		{
			continue;
		}

		const FRetrieveArmorDataRow* ArmorData = ArmorDataTable->FindRow<FRetrieveArmorDataRow>(
			EquippedArmor.ArmorItemId,
			TEXT("AInventoryPreviewActor::UpdateArmorPreview"));
		if (!ArmorData)
		{
			continue;
		}

		for (const FRetrieveArmorVisualPart& VisualPart : ArmorData->VisualParts)
		{
			USkeletalMesh* ArmorMesh = VisualPart.Mesh.LoadSynchronous();
			if (!ArmorMesh)
			{
				continue;
			}

			USkeletalMeshComponent* ArmorMeshComponent = NewObject<USkeletalMeshComponent>(this);
			if (!ArmorMeshComponent)
			{
				continue;
			}

			ArmorMeshComponent->SetSkeletalMesh(ArmorMesh);
			ArmorMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			ArmorMeshComponent->SetGenerateOverlapEvents(false);
			ArmorMeshComponent->SetCastShadow(false);
			ArmorMeshComponent->bCastDynamicShadow = false;
			ArmorMeshComponent->bCastStaticShadow = false;
			ArmorMeshComponent->SetLeaderPoseComponent(LeaderMeshComponent);
			ArmorMeshComponent->AttachToComponent(LeaderMeshComponent, FAttachmentTransformRules::KeepRelativeTransform);

			AddInstanceComponent(ArmorMeshComponent);
			ArmorMeshComponent->RegisterComponent();
			ArmorMeshComponents.Add(ArmorMeshComponent);
		}
	}
}

void AInventoryPreviewActor::HandleEquippedArmorChanged(FGameplayTag EquipmentSlotTag, FName ArmorItemId)
{
	UpdateArmorPreview();

	if (ArmorItemId.IsNone())
	{
		return;
	}

	FGameplayTag MontagePartSlotTag;
	if (UDataTable* ArmorDataTable = ResolveArmorDataTable())
	{
		if (const FRetrieveArmorDataRow* ArmorData = ArmorDataTable->FindRow<FRetrieveArmorDataRow>(
			ArmorItemId,
			TEXT("AInventoryPreviewActor::HandleEquippedArmorChanged")))
		{
			for (const FRetrieveArmorVisualPart& VisualPart : ArmorData->VisualParts)
			{
				if (FindArmorEquipMontage(EquipmentSlotTag, VisualPart.PartSlotTag))
				{
					MontagePartSlotTag = VisualPart.PartSlotTag;
					break;
				}
			}
		}
	}

	PlayArmorEquipMontage(EquipmentSlotTag, MontagePartSlotTag);
}

bool AInventoryPreviewActor::PlayArmorEquipMontage(FGameplayTag EquipmentSlotTag, FGameplayTag PartSlotTag)
{
	const FRetrieveInventoryPreviewArmorMontage* MontageConfig = FindArmorEquipMontage(EquipmentSlotTag, PartSlotTag);
	UAnimMontage* MontageToPlay = MontageConfig && MontageConfig->Montage
		? MontageConfig->Montage.Get()
		: ResolveArmorSlotMontage(EquipmentSlotTag);
	if (!MontageToPlay)
	{
		return false;
	}

	USkeletalMeshComponent* LeaderMeshComponent = ResolvePreviewMeshComponent();
	UAnimInstance* AnimInstance = LeaderMeshComponent ? LeaderMeshComponent->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return false;
	}

	const float PlayRate = MontageConfig && MontageConfig->Montage
		? MontageConfig->PlayRate
		: ArmorEquipMontagePlayRate;
	const float Duration = AnimInstance->Montage_Play(MontageToPlay, FMath::Max(PlayRate, 0.01f));
	if (Duration <= 0.0f)
	{
		return false;
	}

	const FName StartSection = MontageConfig && MontageConfig->Montage
		? MontageConfig->StartSection
		: ArmorEquipMontageStartSection;
	if (!StartSection.IsNone())
	{
		AnimInstance->Montage_JumpToSection(StartSection, MontageToPlay);
	}

	return true;
}

UInventoryComponent* AInventoryPreviewActor::ResolveInventoryComponent() const
{
	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		return PlayerPawn->FindComponentByClass<UInventoryComponent>();
	}

	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (APawn* Pawn = PlayerController->GetPawn())
		{
			return Pawn->FindComponentByClass<UInventoryComponent>();
		}
	}

	return nullptr;
}

UArmorComponent* AInventoryPreviewActor::ResolveArmorComponent() const
{
	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		return PlayerPawn->FindComponentByClass<UArmorComponent>();
	}

	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (APawn* Pawn = PlayerController->GetPawn())
		{
			return Pawn->FindComponentByClass<UArmorComponent>();
		}
	}

	return nullptr;
}

UDataTable* AInventoryPreviewActor::ResolveArmorDataTable() const
{
	if (ArmorDataTableOverride)
	{
		return ArmorDataTableOverride;
	}

	UArmorComponent* ArmorComponent = BoundArmorComponent.Get();
	if (!ArmorComponent)
	{
		ArmorComponent = ResolveArmorComponent();
	}

	return ArmorComponent ? ArmorComponent->GetArmorDataTable() : nullptr;
}

USkeletalMeshComponent* AInventoryPreviewActor::ResolvePreviewMeshComponent() const
{
	TArray<USkeletalMeshComponent*> SkeletalMeshComponents;
	GetComponents(SkeletalMeshComponents);

	if (!PreviewSkeletalMeshComponentName.IsNone())
	{
		for (USkeletalMeshComponent* SkeletalMeshComponent : SkeletalMeshComponents)
		{
			if (SkeletalMeshComponent && SkeletalMeshComponent->GetFName() == PreviewSkeletalMeshComponentName)
			{
				return SkeletalMeshComponent;
			}
		}
	}

	return SkeletalMeshComponents.Num() > 0 ? SkeletalMeshComponents[0] : nullptr;
}

const FRetrieveInventoryPreviewArmorMontage* AInventoryPreviewActor::FindArmorEquipMontage(
	FGameplayTag EquipmentSlotTag,
	FGameplayTag PartSlotTag) const
{
	const FRetrieveInventoryPreviewArmorMontage* SlotFallback = nullptr;
	for (const FRetrieveInventoryPreviewArmorMontage& MontageConfig : ArmorEquipMontages)
	{
		if (!MontageConfig.Montage || MontageConfig.EquipmentSlotTag != EquipmentSlotTag)
		{
			continue;
		}

		if (PartSlotTag.IsValid() && MontageConfig.PartSlotTag == PartSlotTag)
		{
			return &MontageConfig;
		}

		if (!MontageConfig.PartSlotTag.IsValid())
		{
			SlotFallback = &MontageConfig;
		}
	}

	return SlotFallback;
}

UAnimMontage* AInventoryPreviewActor::ResolveArmorSlotMontage(FGameplayTag EquipmentSlotTag) const
{
	if (EquipmentSlotTag == RetrieveGameplayTags::Equipment_Slot_Head && Montage_ArmorHead)
	{
		return Montage_ArmorHead;
	}

	if (EquipmentSlotTag == RetrieveGameplayTags::Equipment_Slot_Chest && Montage_ArmorChest)
	{
		return Montage_ArmorChest;
	}

	if (EquipmentSlotTag == RetrieveGameplayTags::Equipment_Slot_Hands && Montage_ArmorHands)
	{
		return Montage_ArmorHands;
	}

	if (EquipmentSlotTag == RetrieveGameplayTags::Equipment_Slot_Legs && Montage_ArmorLegs)
	{
		return Montage_ArmorLegs;
	}

	if (EquipmentSlotTag == RetrieveGameplayTags::Equipment_Slot_Feet && Montage_ArmorFeet)
	{
		return Montage_ArmorFeet;
	}

	return Montage_ArmorDefault;
}

void AInventoryPreviewActor::BindInventoryEvents()
{
	BoundInventoryComponent = ResolveInventoryComponent();
	BoundArmorComponent = ResolveArmorComponent();

	if (BoundInventoryComponent)
	{
		BoundInventoryComponent->OnEquippedArmorChanged.RemoveDynamic(this, &ThisClass::HandleEquippedArmorChanged);
		BoundInventoryComponent->OnEquippedArmorChanged.AddDynamic(this, &ThisClass::HandleEquippedArmorChanged);
	}
}

void AInventoryPreviewActor::UnbindInventoryEvents()
{
	if (BoundInventoryComponent)
	{
		BoundInventoryComponent->OnEquippedArmorChanged.RemoveDynamic(this, &ThisClass::HandleEquippedArmorChanged);
	}
}
