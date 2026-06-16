
#include "RetrievePawnCosmeticComponent.h"

#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Character/Cosmetics/RetrieveCosmeticData.h"
#include "Character/Cosmetics/RetrieveModularMeshTypes.h"
#include "Character/SovereignCharacter.h"
#include "Components/Player/WeaponComponent.h"
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
		WeaponComp->OnWeaponEquipped.AddUniqueDynamic(this, &ThisClass::OnWeaponEquipped);
		WeaponComp->OnWeaponUnequipped.AddUniqueDynamic(this, &ThisClass::OnWeaponUnequipped);

		SetWeaponTypeTag(WeaponComp->IsEquipped()
			? WeaponComp->GetWeaponData().WeaponTypeTag
			: RetrieveGameplayTags::Weapon_Type_Unarmed);
	}
	RefreshCosmeticState();
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

	if (AActor* Owner = GetOwner())
	{
		if (UWeaponComponent* WeaponComp = Owner->FindComponentByClass<UWeaponComponent>())
		{
			WeaponComp->OnWeaponEquipped.RemoveDynamic(this, &ThisClass::OnWeaponEquipped);
			WeaponComp->OnWeaponUnequipped.RemoveDynamic(this, &ThisClass::OnWeaponUnequipped);
		}
	}

	ClearSpawnedModularParts();
}

void URetrievePawnCosmeticComponent::OnWeaponEquipped(FName WeaponItemId)
{
	if (UWeaponComponent* WeaponComp = GetOwner()->FindComponentByClass<UWeaponComponent>())
	{
		SetWeaponTypeTag(WeaponComp->GetWeaponData().WeaponTypeTag);
	}

	RefreshCosmeticState();
}

void URetrievePawnCosmeticComponent::OnWeaponUnequipped(FName WeaponItemId)
{
	SetWeaponTypeTag(RetrieveGameplayTags::Weapon_Type_Unarmed);

	RefreshCosmeticState();
}

void URetrievePawnCosmeticComponent::SetWeaponTypeTag(FGameplayTag NewWeaponTypeTag)
{
	const FGameplayTag Resolved = NewWeaponTypeTag.IsValid()
		? NewWeaponTypeTag
		: RetrieveGameplayTags::Weapon_Type_Unarmed;

	// 무기 타입은 단일값 축이다. 이전 무기 태그와 기본 Unarmed를 함께 정리하고 새 태그만 남긴다.
	if (CurrentWeaponTypeTag.IsValid())
	{
		CosmeticTags.RemoveTag(CurrentWeaponTypeTag);
	}
	CosmeticTags.RemoveTag(RetrieveGameplayTags::Weapon_Type_Unarmed);

	CurrentWeaponTypeTag = Resolved;
	CosmeticTags.AddTag(Resolved);
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

	RefreshCosmeticState();
}

void URetrievePawnCosmeticComponent::ApplyEquipmentPartsForSlot(
	FGameplayTag EquipmentSlotTag,
	const TArray<FRetrieveArmorVisualPart>& VisualParts,
	const FGameplayTagContainer& SuppressedDefaultPartSlots)
{
	if (!EquipmentSlotTag.IsValid()) { return; }

	RemoveSpawnedEquipmentForSlot(EquipmentSlotTag);

	USkeletalMeshComponent* VisualMesh = GetVisualMeshComponent();
	AActor* Owner = GetOwner();

	if (IsValid(VisualMesh) && IsValid(Owner) && VisualParts.Num() > 0)
	{
		FRetrieveSpawnedEquipmentVisuals& SpawnedVisuals = SpawnedEquipmentVisuals.FindOrAdd(EquipmentSlotTag);
		for (const FRetrieveArmorVisualPart& Part : VisualParts)
		{
			if (!Part.PartSlotTag.IsValid() || Part.Mesh.IsNull()) { continue; }

			// soft 참조이므로 장착 시점에 로드한다. (preload 정책은 후속 단계)
			USkeletalMesh* Mesh = Part.Mesh.LoadSynchronous();
			if (!Mesh) { continue; }

			if (USkeletalMeshComponent* PartComponent = CreateModularPartComponent(Mesh, VisualMesh, Owner, /*bCastShadow=*/true))
			{
				SpawnedVisuals.Components.Add(PartComponent);
			}
		}
		if (SpawnedVisuals.Components.Num() == 0)
		{
			SpawnedEquipmentVisuals.Remove(EquipmentSlotTag);
		}
	}

	ApplyEquipmentSuppressionForSlot(EquipmentSlotTag, VisualParts, SuppressedDefaultPartSlots);
}

void URetrievePawnCosmeticComponent::ClearEquipmentVisualSlot(FGameplayTag EquipmentSlotTag)
{
	if (!EquipmentSlotTag.IsValid()) { return; }

	RemoveSpawnedEquipmentForSlot(EquipmentSlotTag);
	ClearEquipmentSuppressionForSlot(EquipmentSlotTag);
}

void URetrievePawnCosmeticComponent::ClearAllEquipmentVisualSlots()
{
	for (TPair<FGameplayTag, FRetrieveSpawnedEquipmentVisuals>& SpawnedVisuals : SpawnedEquipmentVisuals)
	{
		for (USkeletalMeshComponent* Component : SpawnedVisuals.Value.Components)
		{
			if (Component)
			{
				Component->DestroyComponent();
			}
		}
	}
	SpawnedEquipmentVisuals.Reset();

	// 장비 visual만 제거한다. 기본 바디 파츠는 유지하고 억제만 해제 후 visibility 복원.
	const bool bHadSuppression = SuppressedDefaultPartSlotsByEquipmentSlot.Num() > 0;
	SuppressedDefaultPartSlotsByEquipmentSlot.Reset();
	if (bHadSuppression)
	{
		RefreshDefaultPartVisibility();
	}
}

void URetrievePawnCosmeticComponent::RefreshCosmeticState()
{
	ApplyCosmeticLayer();
	ApplyVisualLayout();
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

void URetrievePawnCosmeticComponent::ApplyVisualLayout()
{
	if (!IsValid(CosmeticData)) { return; }

	URetrieveCharacterVisualLayout* DesiredLayout =
		CosmeticData->VisualLayoutRules.SelectBestLayout(BuildCosmeticTags());
	if (!IsValid(DesiredLayout) || DesiredLayout == CurrentVisualLayout)
	{
		return;
	}

	USkeletalMeshComponent* VisualMesh = GetVisualMeshComponent();
	if (!IsValid(VisualMesh)) { return; }

	if (DesiredLayout->BaseVisualMesh)
	{
		VisualMesh->SetSkeletalMesh(DesiredLayout->BaseVisualMesh);
	}
	CurrentVisualLayout = DesiredLayout;

	// 레이아웃 기준 기본 바디 파츠를 (재)생성하고, 현재 장비 억제 상태로 visibility 갱신
	ApplyDefaultBodyPartSet(DesiredLayout->DefaultBodyPartSet);
	RefreshDefaultPartVisibility();

	// 모듈러 바디가 생성된 경우 통짜 BaseVisualMesh는 숨긴다. 단, VisualMesh는 여전히
	// modular part들의 LeaderPose source이므로, 숨겨져도 포즈를 갱신하도록 설정한다.
	// 자식 파츠까지 같이 숨지 않도록 propagate=false로 둔다.
	const bool bHasModularBody = SpawnedDefaultBodyParts.Num() > 0;
	if (bHasModularBody)
	{
		VisualMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		// 숨겨도 풀바디 그림자는 계속 던지게 한다. 조립 파츠 대신 통짜 메시가 단일 그림자 caster 역할.
		VisualMesh->SetCastHiddenShadow(true);
	}
	VisualMesh->SetVisibility(!bHasModularBody, /*bPropagateToChildren=*/false);

	ApplyMorphTargetsToSpawnedParts();
}

void URetrievePawnCosmeticComponent::ApplyDefaultBodyPartSet(const URetrieveModularPartSet* PartSet)
{
	ClearDefaultBodyParts();

	// 활성 레이아웃의 바디 세트가 곧 현재 성별의 morph 프로파일이다. 바디 + 방어구 파츠에 일괄 적용한다.
	CurrentMorphTargets = PartSet ? PartSet->MorphTargets : TMap<FName, float>();

	if (!PartSet) { return; }

	USkeletalMeshComponent* VisualMesh = GetVisualMeshComponent();
	if (!IsValid(VisualMesh)) { return; }

	AActor* Owner = GetOwner();
	if (!IsValid(Owner)) { return; }

	for (const FRetrieveSkinnedPartMesh& Part : PartSet->SkinnedParts)
	{
		if (!Part.PartSlotTag.IsValid() || !Part.Mesh) { continue; }

		// 같은 슬롯이 중복 정의된 경우 기존 것을 정리하고 마지막 항목으로 덮어쓴다.
		if (TObjectPtr<USkeletalMeshComponent>* Existing = SpawnedDefaultBodyParts.Find(Part.PartSlotTag))
		{
			if (*Existing) { (*Existing)->DestroyComponent(); }
		}

		if (USkeletalMeshComponent* PartComponent = CreateModularPartComponent(Part.Mesh, VisualMesh, Owner, /*bCastShadow=*/false))
		{
			SpawnedDefaultBodyParts.Add(Part.PartSlotTag, PartComponent);
		}
	}
}

void URetrievePawnCosmeticComponent::ClearDefaultBodyParts()
{
	for (TPair<FGameplayTag, TObjectPtr<USkeletalMeshComponent>>& Pair : SpawnedDefaultBodyParts)
	{
		if (Pair.Value)
		{
			Pair.Value->DestroyComponent();
		}
	}
	SpawnedDefaultBodyParts.Reset();
}

void URetrievePawnCosmeticComponent::RefreshDefaultPartVisibility()
{
	FGameplayTagContainer Suppressed;
	for (const TPair<FGameplayTag, FGameplayTagContainer>& Pair : SuppressedDefaultPartSlotsByEquipmentSlot)
	{
		Suppressed.AppendTags(Pair.Value);
	}

	for (const TPair<FGameplayTag, TObjectPtr<USkeletalMeshComponent>>& Pair : SpawnedDefaultBodyParts)
	{
		if (USkeletalMeshComponent* Component = Pair.Value)
		{
			const bool bSuppressed = Suppressed.HasTag(Pair.Key);
			Component->SetVisibility(!bSuppressed, false);
		}
	}
}

void URetrievePawnCosmeticComponent::RemoveSpawnedEquipmentForSlot(FGameplayTag EquipmentSlotTag)
{
	FRetrieveSpawnedEquipmentVisuals* SpawnedVisuals = SpawnedEquipmentVisuals.Find(EquipmentSlotTag);
	if (!SpawnedVisuals) { return; }

	for (USkeletalMeshComponent* Component : SpawnedVisuals->Components)
	{
		if (Component)
		{
			Component->DestroyComponent();
		}
	}

	SpawnedEquipmentVisuals.Remove(EquipmentSlotTag);
}

void URetrievePawnCosmeticComponent::ApplyEquipmentSuppressionForSlot(
	FGameplayTag EquipmentSlotTag,
	const TArray<FRetrieveArmorVisualPart>& VisualParts,
	const FGameplayTagContainer& ExplicitSuppressed)
{
	if (!EquipmentSlotTag.IsValid()) { return; }

	FGameplayTagContainer Suppressed;

	// 자동: 장비가 직접 채우는 슬롯은 기본 파츠와 겹치므로 가린다. (장갑 → 기본 Hand)
	for (const FRetrieveArmorVisualPart& Part : VisualParts)
	{
		if (Part.PartSlotTag.IsValid() && !Part.Mesh.IsNull())
		{
			Suppressed.AddTag(Part.PartSlotTag);
		}
	}

	// 명시: 다른 슬롯 추가 가리기. (투구 → Hair / Attachment.Face)
	Suppressed.AppendTags(ExplicitSuppressed);

	if (Suppressed.IsEmpty())
	{
		SuppressedDefaultPartSlotsByEquipmentSlot.Remove(EquipmentSlotTag);
	}
	else
	{
		SuppressedDefaultPartSlotsByEquipmentSlot.Add(EquipmentSlotTag, Suppressed);
	}

	RefreshDefaultPartVisibility();
}

void URetrievePawnCosmeticComponent::ClearEquipmentSuppressionForSlot(FGameplayTag EquipmentSlotTag)
{
	if (SuppressedDefaultPartSlotsByEquipmentSlot.Remove(EquipmentSlotTag) > 0)
	{
		RefreshDefaultPartVisibility();
	}
}

USkeletalMeshComponent* URetrievePawnCosmeticComponent::CreateModularPartComponent(
	USkeletalMesh* Mesh,
	USkeletalMeshComponent* VisualMesh,
	AActor* Owner,
	bool bCastShadow)
{
	if (!Mesh || !IsValid(VisualMesh) || !IsValid(Owner)) { return nullptr; }

	USkeletalMeshComponent* PartComponent = NewObject<USkeletalMeshComponent>(Owner);
	if (!IsValid(PartComponent)) { return nullptr; }

	PartComponent->SetSkeletalMesh(Mesh);
	PartComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PartComponent->SetGenerateOverlapEvents(false);
	PartComponent->SetCanEverAffectNavigation(false);
	// 조립 파츠가 각자 그림자를 던지면 파츠 경계마다 이음새가 생긴다. 기본 바디 파츠는 그림자를 끄고
	// 숨긴 통짜 VisualMesh가 풀바디 그림자를 대신 던진다. 장비 파츠는 실루엣 반영을 위해 켜 둔다.
	PartComponent->SetCastShadow(bCastShadow);
	PartComponent->AttachToComponent(VisualMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	PartComponent->RegisterComponent();
	PartComponent->SetLeaderPoseComponent(VisualMesh);
	ApplyMorphTargets(PartComponent);

	return PartComponent;
}

void URetrievePawnCosmeticComponent::ApplyMorphTargets(USkeletalMeshComponent* MeshComponent) const
{
	if (!IsValid(MeshComponent)) { return; }

	// 데이터로 주입된 morph만 적용한다. 메시에 해당 morph가 없으면 SetMorphTarget은 무시된다.
	for (const TPair<FName, float>& Morph : CurrentMorphTargets)
	{
		MeshComponent->SetMorphTarget(Morph.Key, Morph.Value);
	}
}

void URetrievePawnCosmeticComponent::ApplyMorphTargetsToSpawnedParts() const
{
	for (const TPair<FGameplayTag, TObjectPtr<USkeletalMeshComponent>>& Pair : SpawnedDefaultBodyParts)
	{
		ApplyMorphTargets(Pair.Value);
	}

	for (const TPair<FGameplayTag, FRetrieveSpawnedEquipmentVisuals>& SpawnedVisuals : SpawnedEquipmentVisuals)
	{
		for (USkeletalMeshComponent* Component : SpawnedVisuals.Value.Components)
		{
			ApplyMorphTargets(Component);
		}
	}
}

void URetrievePawnCosmeticComponent::ClearSpawnedModularParts()
{
	ClearDefaultBodyParts();
	ClearAllEquipmentVisualSlots();
}

USkeletalMeshComponent* URetrievePawnCosmeticComponent::GetVisualMeshComponent() const
{
	if (const ASovereignCharacter* Character = Cast<ASovereignCharacter>(GetOwner()))
	{
		return Character->GetVisualMesh();
	}
	return nullptr;
}

FGameplayTagContainer URetrievePawnCosmeticComponent::BuildCosmeticTags() const
{
	return CosmeticTags;
}
