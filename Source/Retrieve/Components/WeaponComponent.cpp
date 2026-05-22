#include "Components/WeaponComponent.h"

#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Components/RetrievePawnExtensionComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"

UWeaponComponent::UWeaponComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UWeaponComponent, CurrentWeaponDataRow);
}

void UWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnequipWeapon();
	Super::EndPlay(EndPlayReason);
}

bool UWeaponComponent::EquipWeapon(FName WeaponItemId)
{
	if (!HasAuthorityToModify())
	{
		return false;
	}

	const FRetrieveWeaponDataRow* WeaponData = FindWeaponData(WeaponItemId);
	if (!WeaponData)
	{
		return false;
	}

	if (CurrentWeaponDataRow == WeaponItemId)
	{
		return true;
	}

	// 무기별 어빌리티와 메시가 남지 않도록 기존 장착 먼저 정리
	UnequipWeapon();
	return ApplyWeaponData(WeaponItemId, *WeaponData);
}

void UWeaponComponent::UnequipWeapon()
{
	const FName PreviousWeaponId = CurrentWeaponDataRow;

	ClearGrantedWeaponAbilities();
	ClearWeaponVisuals();

	CurrentWeaponDataRow = NAME_None;
	CurrentWeaponData = FRetrieveWeaponDataRow();
	CurrentWeaponTypeTag = FGameplayTag();
	CurrentWeaponAffinityTag = FGameplayTag();
	CurrentWeaponAttackTable = nullptr;

	if (!PreviousWeaponId.IsNone())
	{
		OnWeaponUnequipped.Broadcast(PreviousWeaponId);
	}
}

void UWeaponComponent::OnRep_CurrentWeaponDataRow()
{
	const FName ReplicatedWeaponId = CurrentWeaponDataRow;

	// 클라이언트는 복제된 RowName 기준으로 비주얼과 UI용 캐시만 갱신
	ClearWeaponVisuals();
	CurrentWeaponData = FRetrieveWeaponDataRow();
	CurrentWeaponTypeTag = FGameplayTag();
	CurrentWeaponAffinityTag = FGameplayTag();
	CurrentWeaponAttackTable = nullptr;

	if (ReplicatedWeaponId.IsNone())
	{
		OnWeaponUnequipped.Broadcast(ReplicatedWeaponId);
		return;
	}

	if (const FRetrieveWeaponDataRow* WeaponData = FindWeaponData(ReplicatedWeaponId))
	{
		ApplyWeaponData(ReplicatedWeaponId, *WeaponData);
	}
}

URetrieveAbilitySystemComponent* UWeaponComponent::GetRetrieveAbilitySystemComponent() const
{
	const AActor* Owner = GetOwner();
	const URetrievePawnExtensionComponent* PawnExt = Owner
		? URetrievePawnExtensionComponent::FindPawnExtensionComponent(Owner)
		: nullptr;

	return PawnExt ? PawnExt->GetRetrieveAbilitySystemComponent() : nullptr;
}

const FRetrieveWeaponDataRow* UWeaponComponent::FindWeaponData(FName WeaponItemId) const
{
	if (!WeaponDataTable || WeaponItemId.IsNone())
	{
		return nullptr;
	}

	return WeaponDataTable->FindRow<FRetrieveWeaponDataRow>(WeaponItemId, TEXT("UWeaponComponent::FindWeaponData"));
}

void UWeaponComponent::ClearGrantedWeaponAbilities()
{
	// 서버에서 부여한 무기 전용 어빌리티만 회수
	if (URetrieveAbilitySystemComponent* ASC = GetRetrieveAbilitySystemComponent())
	{
		WeaponGrantedHandles.TakeFromAbilitySystem(ASC);
	}
}

void UWeaponComponent::ClearWeaponVisuals()
{
	for (UMeshComponent* MeshComponent : EquippedWeaponMeshComponents)
	{
		if (MeshComponent)
		{
			MeshComponent->DestroyComponent();
		}
	}
	EquippedWeaponMeshComponents.Reset();
}

bool UWeaponComponent::HasAuthorityToModify() const
{
	const AActor* Owner = GetOwner();
	return !Owner || Owner->HasAuthority();
}

bool UWeaponComponent::ApplyWeaponData(FName WeaponItemId, const FRetrieveWeaponDataRow& WeaponData)
{
	if (HasAuthorityToModify())
	{
		// AbilitySet 부여는 서버에서만 처리. 클라이언트 OnRep 경로는 비주얼만 갱신
		if (URetrieveAbilitySystemComponent* ASC = GetRetrieveAbilitySystemComponent())
		{
			if (URetrieveAbilitySet* AbilitySet = Cast<URetrieveAbilitySet>(WeaponData.WeaponAbilitySet.TryLoad()))
			{
				AbilitySet->GiveToAbilitySystem(ASC, &WeaponGrantedHandles, GetOwner());
			}
		}
	}

	CurrentWeaponDataRow = WeaponItemId;
	CurrentWeaponData = WeaponData;
	CurrentWeaponTypeTag = WeaponData.WeaponTypeTag;
	CurrentWeaponAffinityTag = WeaponData.WeaponAffinityTag;
	CurrentWeaponAttackTable = WeaponData.WeaponAttackTable.LoadSynchronous();

	ApplyWeaponVisuals(WeaponData);
	OnWeaponEquipped.Broadcast(CurrentWeaponDataRow);
	return true;
}

bool UWeaponComponent::ApplyWeaponVisuals(const FRetrieveWeaponDataRow& WeaponData)
{
	if (WeaponData.Attachments.IsEmpty())
	{
		return false;
	}

	bool bAttachedAnyPart = false;
	for (const FRetrieveWeaponAttachmentData& Attachment : WeaponData.Attachments)
	{
		UMeshComponent* WeaponMeshComponent = CreateWeaponMeshComponent(Attachment);
		if (!WeaponMeshComponent)
		{
			continue;
		}

		USceneComponent* AttachParent = FindAttachmentParent(Attachment);
		if (!AttachParent)
		{
			WeaponMeshComponent->DestroyComponent();
			continue;
		}

		WeaponMeshComponent->RegisterComponent();
		WeaponMeshComponent->AttachToComponent(
			AttachParent,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			Attachment.AttachSocketName);
		WeaponMeshComponent->SetRelativeTransform(Attachment.RelativeTransform);

		EquippedWeaponMeshComponents.Add(WeaponMeshComponent);
		bAttachedAnyPart = true;
	}

	return bAttachedAnyPart;
}

UMeshComponent* UWeaponComponent::CreateWeaponMeshComponent(const FRetrieveWeaponAttachmentData& Attachment) const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	if (Attachment.MeshType == ERetrieveWeaponMeshType::StaticMesh)
	{
		UStaticMesh* Mesh = Attachment.StaticMesh.LoadSynchronous();
		if (!Mesh)
		{
			return nullptr;
		}
		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(Owner);
		Comp->SetStaticMesh(Mesh);
		return Comp;
	}

	USkeletalMesh* Mesh = Attachment.SkeletalMesh.LoadSynchronous();
	if (!Mesh)
	{
		return nullptr;
	}
	USkeletalMeshComponent* Comp = NewObject<USkeletalMeshComponent>(Owner);
	Comp->SetSkeletalMesh(Mesh);
	return Comp;
}

USceneComponent* UWeaponComponent::FindAttachmentParent(const FRetrieveWeaponAttachmentData& Attachment) const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	// 이름·태그 기반 컴포넌트 검색 공통 로직
	auto FindComponent = [&](auto Predicate) -> USceneComponent*
	{
		TArray<USceneComponent*> Components;
		Owner->GetComponents<USceneComponent>(Components);
		for (USceneComponent* Comp : Components)
		{
			if (Comp && Predicate(Comp))
			{
				return Comp;
			}
		}
		return nullptr;
	};

	switch (Attachment.AttachTarget)
	{
	case ERetrieveWeaponAttachTarget::OwnerRoot:
		return Owner->GetRootComponent();

	case ERetrieveWeaponAttachTarget::OwnerComponentName:
		return FindComponent([&](USceneComponent* C) { return C->GetFName() == Attachment.AttachComponentName; });

	case ERetrieveWeaponAttachTarget::OwnerComponentTag:
		return FindComponent([&](USceneComponent* C) { return C->ComponentHasTag(Attachment.AttachComponentTag); });

	default: // CharacterMeshSocket — 캐릭터 메시에 직접 소켓으로 붙임
		ACharacter* CharacterOwner = Cast<ACharacter>(Owner);
		return CharacterOwner ? CharacterOwner->GetMesh() : nullptr;
	}
}
