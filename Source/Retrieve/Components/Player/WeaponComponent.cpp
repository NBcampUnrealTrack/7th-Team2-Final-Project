#include "Components/Player/WeaponComponent.h"

#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Character/Cosmetics/RetrieveAlsLinkedAnimInstance.h"
#include "Components/Pawn/RetrievePawnExtensionComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "GameplayTags/RetrieveGameplayTags.h"
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

	ClearWeaponData(); // 이전 무기 '데이터만' 정리 (OLD 메시는 유지 — 교체 연출이 넘겨받음)
	if (!ApplyWeaponData(WeaponItemId, *WeaponData))
	{
		return false;
	}

	// NEW 레이어로 먼저 relink → GA가 NEW EquipMontage를 읽도록 broadcast를 트리거보다 앞에 둔다.
	OnWeaponEquipped.Broadcast(CurrentWeaponDataRow);

	// 연출/비주얼은 GA가. 트리거 실패(미부여/몽타주 없음)면 즉시 데이터로 리빌드.
	if (!TryTriggerEquipTransition(RetrieveGameplayTags::GameplayEvent_Player_EquipWeapon))
	{
		ReconcileVisuals();
	}
	return true;
}

void UWeaponComponent::UnequipWeapon()
{
	const FName PreviousWeaponId = CurrentWeaponDataRow;
	if (PreviousWeaponId.IsNone())
	{
		return;
	}

	ClearWeaponData();

	// 해제 연출은 '벗는' 무기(=현재 링크된 레이어)의 UnequipMontage.
	// relink(Unarmed) 전에 트리거해 레이어가 살아있을 때 참조를 잡게 한다.
	const bool bTriggered = TryTriggerEquipTransition(RetrieveGameplayTags::GameplayEvent_Player_UnequipWeapon);

	OnWeaponUnequipped.Broadcast(PreviousWeaponId); // → Cosmetic이 Unarmed로 relink

	if (!bTriggered)
	{
		ReconcileVisuals();
	}
}

void UWeaponComponent::ClearWeaponData()
{
	// 무기 공격력 GE 먼저 제거 (ClearGrantedWeaponAbilities 전에 수행)
	if (HasAuthorityToModify() && WeaponAttackPowerEffectHandle.IsValid())
	{
		if (URetrieveAbilitySystemComponent* ASC = GetRetrieveAbilitySystemComponent())
		{
			ASC->RemoveActiveGameplayEffect(WeaponAttackPowerEffectHandle);
		}
		WeaponAttackPowerEffectHandle = FActiveGameplayEffectHandle();
	}

	ClearGrantedWeaponAbilities();

	CurrentWeaponDataRow = NAME_None;
	CurrentWeaponData = FRetrieveWeaponDataRow();
	CurrentWeaponTypeTag = FGameplayTag();
	CurrentWeaponAffinityTag = FGameplayTag();
	CurrentWeaponAttackTable = nullptr;
}

void UWeaponComponent::SpawnWeaponVisuals()
{
	if (IsEquipped())
	{
		// 손 소켓에 스폰. 납검 상태 보정은 CombatStance가 OnWeaponEquipped에서 SetWeaponDrawn으로 처리.
		ApplyWeaponVisuals(CurrentWeaponData);
	}
}

void UWeaponComponent::ReconcileVisuals()
{
	ClearWeaponVisuals();
	SpawnWeaponVisuals();
}

bool UWeaponComponent::TryTriggerEquipTransition(const FGameplayTag& EventTag)
{
	URetrieveAbilitySystemComponent* ASC = GetRetrieveAbilitySystemComponent();
	if (!ASC)
	{
		return false;
	}

	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = GetOwner();
	return ASC->HandleGameplayEvent(EventTag, &Payload) > 0;
}

void UWeaponComponent::OnRep_CurrentWeaponDataRow()
{
	const FName ReplicatedWeaponId = CurrentWeaponDataRow;

	// 클라이언트는 복제된 RowName 기준으로 비주얼과 UI용 캐시만 갱신
	CurrentWeaponData = FRetrieveWeaponDataRow();
	CurrentWeaponTypeTag = FGameplayTag();
	CurrentWeaponAffinityTag = FGameplayTag();
	CurrentWeaponAttackTable = nullptr;

	if (ReplicatedWeaponId.IsNone())
	{
		ClearWeaponVisuals();
		OnWeaponUnequipped.Broadcast(ReplicatedWeaponId);
		return;
	}

	if (const FRetrieveWeaponDataRow* WeaponData = FindWeaponData(ReplicatedWeaponId))
	{
		ApplyWeaponData(ReplicatedWeaponId, *WeaponData); // 클라: 캐시만 (어빌리티/GE는 권위 가드)
		OnWeaponEquipped.Broadcast(ReplicatedWeaponId);   // → Cosmetic relink
		ReconcileVisuals();                               // 원격 즉시 스폰 (연출 생략)
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
	WeaponAttachParts.Reset();
}

UMeshComponent* UWeaponComponent::GetPrimaryEquippedWeaponMesh() const
{
	for (UMeshComponent* MeshComponent : EquippedWeaponMeshComponents)
	{
		if (IsValid(MeshComponent))
		{
			return MeshComponent;
		}
	}
	return nullptr;
}

UMeshComponent* UWeaponComponent::GetWeaponMeshForTrace(FName StartSocket, FName EndSocket) const
{
	if (!StartSocket.IsNone() && !EndSocket.IsNone())
	{
		for (UMeshComponent* MeshComponent : EquippedWeaponMeshComponents)
		{
			if (IsValid(MeshComponent)
				&& MeshComponent->DoesSocketExist(StartSocket)
				&& MeshComponent->DoesSocketExist(EndSocket))
			{
				return MeshComponent;
			}
		}
	}
	
	return GetPrimaryEquippedWeaponMesh();
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
		// AbilitySet 부여와 무기 공격력 GE 적용은 서버에서만 처리
		// 클라이언트 OnRep 경로는 비주얼만 갱신
		if (URetrieveAbilitySystemComponent* ASC = GetRetrieveAbilitySystemComponent())
		{
			if (URetrieveAbilitySet* AbilitySet = Cast<URetrieveAbilitySet>(WeaponData.WeaponAbilitySet.TryLoad()))
			{
				AbilitySet->GiveToAbilitySystem(ASC, &WeaponGrantedHandles, GetOwner());
			}

			// 무기 AttackPower를 캐릭터 어트리뷰트에 가산
			// GE_WeaponAttackPower: Infinite, Add on AttackPower, SetByCaller(Data.Weapon.AttackPower)
			if (WeaponAttackPowerEffect && WeaponData.AttackPower > 0.0f)
			{
				FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
				EffectContext.AddSourceObject(this);
				const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(
					WeaponAttackPowerEffect, 1.0f, EffectContext);
				if (SpecHandle.IsValid())
				{
					SpecHandle.Data->SetSetByCallerMagnitude(
						RetrieveGameplayTags::Data_Weapon_AttackPower,
						WeaponData.AttackPower);
					WeaponAttackPowerEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
				}
			}
		}
	}

	CurrentWeaponDataRow = WeaponItemId;
	CurrentWeaponData = WeaponData;
	CurrentWeaponTypeTag = WeaponData.WeaponTypeTag;
	CurrentWeaponAffinityTag = WeaponData.WeaponAffinityTag;
	CurrentWeaponAttackTable = WeaponData.WeaponAttackTable.LoadSynchronous();

	// 비주얼 스폰과 OnWeaponEquipped 브로드캐스트는 호출자(EquipWeapon / OnRep)가 담당한다.
	// (교체 연출 타이밍 제어 + relink 순서 보장을 위해 데이터 적용과 분리)
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

		// 발검/납검 소켓 스왑용 기록(손=AttachSocketName, 오프셋 보존). 등 소켓은 SetWeaponDrawn에서 레이어 맵으로 해석.
		FRetrieveEquippedWeaponPart& Part = WeaponAttachParts.AddDefaulted_GetRef();
		Part.Mesh = WeaponMeshComponent;
		Part.DrawnSocket = Attachment.AttachSocketName;
		Part.RelativeTransform = Attachment.RelativeTransform;

		bAttachedAnyPart = true;
	}

	return bAttachedAnyPart;
}

void UWeaponComponent::SetWeaponDrawn(bool bDrawn)
{
	// 등(수납) 소켓은 무기 타입 레이어가 소유(drawn→sheathed 맵). 발검은 Part의 손 소켓을 그대로 쓰므로 조회 불필요.
	// spawn 시점이 아니라 여기서 즉석 조회한다(init 때 무기 장착이 cosmetic relink보다 앞서 캐싱하면 None으로 굳음).
	const URetrieveAlsLinkedAnimInstance* Layer = nullptr;
	if (!bDrawn)
	{
		if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
		{
			if (const USkeletalMeshComponent* CharacterMesh = Character->GetMesh())
			{
				for (UAnimInstance* Linked : CharacterMesh->GetLinkedAnimInstances())
				{
					if (const URetrieveAlsLinkedAnimInstance* WeaponLayer = Cast<URetrieveAlsLinkedAnimInstance>(Linked))
					{
						Layer = WeaponLayer;
						break;
					}
				}
			}
		}
	}

	for (const FRetrieveEquippedWeaponPart& Part : WeaponAttachParts)
	{
		UMeshComponent* Mesh = Part.Mesh;
		if (!IsValid(Mesh))
		{
			continue;
		}

		// 납검 소켓 매핑이 없는 파트는 그대로 둔다(예: 항상 손에 있는 무기 등).
		const FName TargetSocket = bDrawn
			? Part.DrawnSocket
			: (Layer ? Layer->SheathedSocketByDrawnSocket.FindRef(Part.DrawnSocket) : NAME_None);
		if (TargetSocket.IsNone())
		{
			continue;
		}

		USceneComponent* AttachParent = Mesh->GetAttachParent();
		if (!AttachParent)
		{
			continue;
		}

		Mesh->AttachToComponent(AttachParent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TargetSocket);
		Mesh->SetRelativeTransform(Part.RelativeTransform); // SnapToTarget이 0으로 만든 오프셋 복원
	}
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
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetGenerateOverlapEvents(false);
		Comp->SetCanEverAffectNavigation(false);
		return Comp;
	}

	USkeletalMesh* Mesh = Attachment.SkeletalMesh.LoadSynchronous();
	if (!Mesh)
	{
		return nullptr;
	}
	USkeletalMeshComponent* Comp = NewObject<USkeletalMeshComponent>(Owner);
	Comp->SetSkeletalMesh(Mesh);
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->SetGenerateOverlapEvents(false);
	Comp->SetCanEverAffectNavigation(false);
	return Comp;
}

USceneComponent* UWeaponComponent::FindAttachmentParent(
	const FRetrieveWeaponAttachmentData& Attachment) const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	auto HasSocket = [&](USceneComponent* Comp)
	{
		return Comp &&
			(Attachment.AttachSocketName.IsNone()
			|| Comp->DoesSocketExist(Attachment.AttachSocketName));
	};

	TArray<USceneComponent*> SceneComponents;
	Owner->GetComponents<USceneComponent>(SceneComponents);

	auto FindSceneComponent = [&](TFunctionRef<bool(USceneComponent*)> Predicate)
	{
		for (USceneComponent* Comp : SceneComponents)
		{
			if (Comp && Predicate(Comp) && HasSocket(Comp))
			{
				return Comp;
			}
		}
		return static_cast<USceneComponent*>(nullptr);
	};

	switch (Attachment.AttachTarget)
	{
	case ERetrieveWeaponAttachTarget::OwnerRoot:
		return Owner->GetRootComponent();

	case ERetrieveWeaponAttachTarget::OwnerComponentName:
		return FindSceneComponent([&](USceneComponent* Comp)
		{
			return Comp->GetFName() == Attachment.AttachComponentName;
		});

	case ERetrieveWeaponAttachTarget::OwnerComponentTag:
		return FindSceneComponent([&](USceneComponent* Comp)
		{
			return Comp->ComponentHasTag(Attachment.AttachComponentTag);
		});

	default:
		break;
	}

	ACharacter* CharacterOwner = Cast<ACharacter>(Owner);
	if (!CharacterOwner)
	{
		return nullptr;
	}

	if (!Attachment.AttachComponentName.IsNone())
	{
		if (USceneComponent* Comp = FindSceneComponent([&](USceneComponent* C)
		{
			return C->GetFName() == Attachment.AttachComponentName;
		}))
		{
			return Comp;
		}
	}

	// 단일 메시 구조: 무기는 항상 leader 스켈레톤(GetMesh)의 소켓에 확정 부착한다.
	// 모듈러 파츠는 같은 스켈레톤을 LeaderPose로 공유하므로 파츠를 고르면 소켓이 중복 매칭된다.
	return HasSocket(CharacterOwner->GetMesh()) ? CharacterOwner->GetMesh() : nullptr;
}
