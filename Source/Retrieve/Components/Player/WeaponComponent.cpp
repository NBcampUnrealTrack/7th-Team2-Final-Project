#include "Components/Player/WeaponComponent.h"

#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Components/Pawn/RetrievePawnExtensionComponent.h"
#include "Components/SceneComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Settings/RetrieveWeaponSocketSettings.h"

UWeaponComponent::UWeaponComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	EnhancementVFXTier1 = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Retrieve/VFX/Weapons/Enhancement/NS_WeaponEnhance_Tier1.NS_WeaponEnhance_Tier1")));
	EnhancementVFXTier2 = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Retrieve/VFX/Weapons/Enhancement/NS_WeaponEnhance_Tier2.NS_WeaponEnhance_Tier2")));
	EnhancementVFXTier3 = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Retrieve/VFX/Weapons/Enhancement/NS_WeaponEnhance_Tier3.NS_WeaponEnhance_Tier3")));
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

	ClearWeaponData(); // 이전 무기 '데이터만' 정리 (OLD 메시는 Pending으로 넘겨 교체 연출이 끝낼 때 파괴)
	if (!ApplyWeaponData(WeaponItemId, *WeaponData))
	{
		return false;
	}

	// OLD 메시를 NEW 스폰과 분리해 Pending으로 옮긴다. 즉시 숨겨 NEW(노티로 등장)와 겹치지 않게 하고,
	// 실제 파괴는 교체 몽타주 끝(또는 fallback 즉시)으로 미룬다.
	for (const FRetrieveEquippedWeaponMesh& OldPart : EquippedWeaponMeshComponents)
	{
		if (OldPart.Mesh)
		{
			OldPart.Mesh->SetVisibility(false, /*bPropagateToChildren=*/true);
			PendingDestroyMeshComponents.Add(OldPart.Mesh);
		}
	}
	EquippedWeaponMeshComponents.Reset();
	WeaponAttachParts.Reset();

	// NEW 레이어로 먼저 relink → GA가 NEW EquipMontage를 읽도록 broadcast를 트리거보다 앞에 둔다.
	OnWeaponEquipped.Broadcast(CurrentWeaponDataRow);

	// 연출/비주얼은 GA가. 트리거 실패(미부여/몽타주 없음)면 OLD 즉시 파괴 + 데이터로 리빌드.
	if (!TryTriggerEquipTransition(RetrieveGameplayTags::GameplayEvent_Player_EquipWeapon))
	{
		DestroyPendingVisuals();
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
}

void UWeaponComponent::SpawnWeaponVisuals()
{
	if (IsEquipped())
	{
		// 손 소켓에 스폰. 납검 상태 보정은 스폰 직후 OnWeaponVisualsSpawned을 받는 CombatStance가
		// SetWeaponDrawn으로 처리한다(이 신호는 모든 스폰 경로의 단일 통로 — fallback/OnRep/장착 노티 공통).
		// Equip 전환 중이면 숨겨서 스폰 → 발검 노티가 손에 부착하며 보이게 한다(검→방패 순차 등장).
		ApplyWeaponVisuals(CurrentWeaponData, /*bSpawnHidden=*/IsEquipTransitionActive());
		SpawnWeaponEnhancementVFX();
		OnWeaponVisualsSpawned.Broadcast();
	}
}

void UWeaponComponent::ReconcileVisuals()
{
	ClearWeaponVisuals();
	SpawnWeaponVisuals();
}

void UWeaponComponent::DestroyPendingVisuals()
{
	for (UMeshComponent* MeshComponent : PendingDestroyMeshComponents)
	{
		if (MeshComponent)
		{
			MeshComponent->DestroyComponent();
		}
	}
	PendingDestroyMeshComponents.Reset();
}

void UWeaponComponent::FinalizeEquipTransitionVisuals()
{
	if (IsEquipped())
	{
		// Equip 완료 — draw 노티가 블렌드로 누락됐을 수 있으니 전 파트를 보이게 강제(손 소켓에 스폰돼 있음).
		for (const FRetrieveEquippedWeaponPart& Part : WeaponAttachParts)
		{
			if (IsValid(Part.Mesh))
			{
				Part.Mesh->SetVisibility(true, /*bPropagateToChildren=*/true);
			}
		}
	}
	else
	{
		// Unequip 완료 — 파괴 노티가 누락됐을 수 있으니 메시 정리 보장.
		ClearWeaponVisuals();
	}
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
	ClearWeaponEnhancementVFX();
	for (const FRetrieveEquippedWeaponMesh& Part : EquippedWeaponMeshComponents)
	{
		if (Part.Mesh)
		{
			Part.Mesh->DestroyComponent();
		}
	}
	EquippedWeaponMeshComponents.Reset();
	WeaponAttachParts.Reset();
	NockedArrowMeshes.Reset();
	bArrowNocked = false;
}

void UWeaponComponent::SetNockedArrowVisible(bool bVisible)
{
	bArrowNocked = bVisible;
	for (const TObjectPtr<UMeshComponent>& Arrow : NockedArrowMeshes)
	{
		if (IsValid(Arrow))
		{
			Arrow->SetVisibility(bVisible, /*bPropagateToChildren=*/true);
		}
	}
}

UMeshComponent* UWeaponComponent::GetPrimaryEquippedWeaponMesh() const
{
	for (const FRetrieveEquippedWeaponMesh& Part : EquippedWeaponMeshComponents)
	{
		if (IsValid(Part.Mesh))
		{
			return Part.Mesh;
		}
	}
	return nullptr;
}

UMeshComponent* UWeaponComponent::GetWeaponMeshForTrace(FName StartSocket, FName EndSocket) const
{
	if (!StartSocket.IsNone() && !EndSocket.IsNone())
	{
		for (const FRetrieveEquippedWeaponMesh& Part : EquippedWeaponMeshComponents)
		{
			if (IsValid(Part.Mesh)
				&& Part.Mesh->DoesSocketExist(StartSocket)
				&& Part.Mesh->DoesSocketExist(EndSocket))
			{
				return Part.Mesh;
			}
		}
	}
	
	return GetPrimaryEquippedWeaponMesh();
}

void UWeaponComponent::GetHitVolumeMeshes(TArray<FRetrieveEquippedWeaponMesh>& OutParts) const
{
	OutParts.Reset();
	for (const FRetrieveEquippedWeaponMesh& Part : EquippedWeaponMeshComponents)
	{
		if (Part.bGeneratesHitVolume && IsValid(Part.Mesh))
		{
			OutParts.Add(Part);
		}
	}
}

UMeshComponent* UWeaponComponent::GetEquippedMeshBySocket(FName AttachSocketName) const
{
	if (AttachSocketName.IsNone())
	{
		return nullptr;
	}
	for (const FRetrieveEquippedWeaponPart& Part : WeaponAttachParts)
	{
		if (Part.DrawnSocket == AttachSocketName && IsValid(Part.Mesh))
		{
			return Part.Mesh;
		}
	}
	return nullptr;
}

bool UWeaponComponent::HasAuthorityToModify() const
{
	const AActor* Owner = GetOwner();
	return !Owner || Owner->HasAuthority();
}

bool UWeaponComponent::IsEquipTransitionActive() const
{
	const UAbilitySystemComponent* ASC = GetRetrieveAbilitySystemComponent();
	return IsValid(ASC) && ASC->HasMatchingGameplayTag(RetrieveGameplayTags::Ability_Player_EquipTransition);
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

	// 비주얼 스폰과 OnWeaponEquipped 브로드캐스트는 호출자(EquipWeapon / OnRep)가 담당한다.
	// (교체 연출 타이밍 제어 + relink 순서 보장을 위해 데이터 적용과 분리)
	return true;
}

bool UWeaponComponent::ApplyWeaponVisuals(const FRetrieveWeaponDataRow& WeaponData, bool bSpawnHidden)
{
	if (WeaponData.Attachments.IsEmpty())
	{
		return false;
	}

	// 등(수납) 소켓을 스폰 시점에 무기 타입으로 1번 해석해 파트에 캐싱한다.
	// (이후 SetWeaponDrawn은 파트값만 쓰므로 Unequip으로 타입 태그가 지워져도 납검 위치가 유지된다)
	const URetrieveWeaponSocketSettings* SocketSettings = GetDefault<URetrieveWeaponSocketSettings>();
	const FRetrieveSheathedSocketMap* SheathedMap = SocketSettings
		? SocketSettings->SocketsByWeaponType.Find(CurrentWeaponTypeTag)
		: nullptr;

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
		if (bSpawnHidden)
		{
			WeaponMeshComponent->SetVisibility(false, /*bPropagateToChildren=*/true);
		}
		
		FRetrieveEquippedWeaponMesh& MeshPart = EquippedWeaponMeshComponents.AddDefaulted_GetRef();
		MeshPart.Mesh = WeaponMeshComponent;
		MeshPart.bGeneratesHitVolume = Attachment.bGeneratesHitVolume;
		MeshPart.bUseBoundsTrace = Attachment.bUseBoundsTrace;
		MeshPart.BoundsTraceShape = Attachment.BoundsTraceShape;
		MeshPart.BoundsRadiusScale = Attachment.BoundsRadiusScale;
		MeshPart.BoundsLengthPadding = Attachment.BoundsLengthPadding;
		MeshPart.TraceStartSocket = Attachment.TraceStartSocketNameOverride;
		MeshPart.TraceEndSocket = Attachment.TraceEndSocketNameOverride;

		// 발검/납검 소켓 스왑용 기록(손=AttachSocketName, 오프셋 보존). 등 소켓은 SetWeaponDrawn에서 레이어 맵으로 해석.
		FRetrieveEquippedWeaponPart& Part = WeaponAttachParts.AddDefaulted_GetRef();
		Part.Mesh = WeaponMeshComponent;
		Part.DrawnSocket = Attachment.AttachSocketName;
		Part.SheathedSocket = SheathedMap ? SheathedMap->DrawnToSheathed.FindRef(Attachment.AttachSocketName) : NAME_None;
		Part.RelativeTransform = Attachment.RelativeTransform;

		// 노킹 화살 스폰 가시성은 데이터로: 무한(항상 노킹)=visible, 유한=hidden(Reload 노티가 표시).
		if (Attachment.bIsNockedArrow)
		{
			WeaponMeshComponent->SetVisibility(Attachment.bNockedArrowStartsVisible, /*bPropagateToChildren=*/true);
			NockedArrowMeshes.Add(WeaponMeshComponent);
			if (Attachment.bNockedArrowStartsVisible)
			{
				bArrowNocked = true; // 스폰부터 노킹 → GA가 장전 스킵(무한 활)
			}
		}

		bAttachedAnyPart = true;
	}

	return bAttachedAnyPart;
}

void UWeaponComponent::SetWeaponDrawn(bool bDrawn, FName OnlyDrawnSocket, bool bSetHidden)
{
	for (const FRetrieveEquippedWeaponPart& Part : WeaponAttachParts)
	{
		// 특정 파트만 지정된 경우(검/방패 타이밍 분리) 나머지는 건너뛴다. None이면 전체 처리.
		if (!OnlyDrawnSocket.IsNone() && Part.DrawnSocket != OnlyDrawnSocket)
		{
			continue;
		}

		UMeshComponent* Mesh = Part.Mesh;
		if (!IsValid(Mesh))
		{
			continue;
		}

		// 숨김 지시(등 소켓 없는 방패 등): 소켓 스왑 없이 Hidden 처리. 곧 ClearWeapon이 파괴한다.
		if (bSetHidden)
		{
			Mesh->SetVisibility(false, /*bPropagateToChildren=*/true);
			continue;
		}

		// 이 호출이 대상한 파트는 보이게 한다(Equip 중 hidden 스폰 → 발검 노티가 여기서 등장).
		Mesh->SetVisibility(true, /*bPropagateToChildren=*/true);

		// 납검 소켓 매핑이 없는 파트는 그대로 둔다(예: 항상 손에 있는 무기 등).
		const FName TargetSocket = bDrawn ? Part.DrawnSocket : Part.SheathedSocket;
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

void UWeaponComponent::ClearWeaponEnhancementVFX()
{
	for (UNiagaraComponent* Component : WeaponEnhancementVFXComponents)
	{
		if (IsValid(Component))
		{
			Component->DeactivateImmediate();
			Component->DestroyComponent();
		}
	}
	WeaponEnhancementVFXComponents.Reset();
}

void UWeaponComponent::SpawnWeaponEnhancementVFX()
{
	ClearWeaponEnhancementVFX();

	if (CurrentWeaponData.EnhancementLevel <= 0)
	{
		return;
	}

	UStaticMeshComponent* TargetMesh = nullptr;
	for (const FRetrieveEquippedWeaponMesh& Part : EquippedWeaponMeshComponents)
	{
		UStaticMeshComponent* StaticPart = Cast<UStaticMeshComponent>(Part.Mesh);
		if (!IsValid(StaticPart) || !StaticPart->GetStaticMesh())
		{
			continue;
		}
		if (!TargetMesh || Part.bGeneratesHitVolume)
		{
			TargetMesh = StaticPart;
			if (Part.bGeneratesHitVolume)
			{
				break;
			}
		}
	}
	if (!TargetMesh)
	{
		return; // 원본 Aura 시스템이 StaticMesh 입력형이라 스켈레탈 무기는 제외한다.
	}

	const int32 Level = CurrentWeaponData.EnhancementLevel;
	const TSoftObjectPtr<UNiagaraSystem>& SystemPtr = Level >= 7
		? EnhancementVFXTier3
		: (Level >= 4 ? EnhancementVFXTier2 : EnhancementVFXTier1);
	UNiagaraSystem* System = SystemPtr.LoadSynchronous();
	if (!System)
	{
		return;
	}

	UNiagaraComponent* VFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
		System,
		TargetMesh,
		NAME_None,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		EAttachLocation::KeepRelativeOffset,
		/*bAutoDestroy=*/false,
		/*bAutoActivate=*/false);
	if (!VFX)
	{
		return;
	}

	const FLinearColor AuraColor = CurrentWeaponAffinityTag == RetrieveGameplayTags::Weapon_Affinity_Fire
		? FLinearColor(8.f, 0.35f, 0.03f, 1.f)
		: CurrentWeaponAffinityTag == RetrieveGameplayTags::Weapon_Affinity_Water
			? FLinearColor(0.05f, 2.5f, 10.f, 1.f)
			: CurrentWeaponAffinityTag == RetrieveGameplayTags::Weapon_Affinity_Wind
				? FLinearColor(0.15f, 8.f, 1.2f, 1.f)
				: FLinearColor(3.5f, 1.2f, 7.f, 1.f);
	const float TierAlpha = FMath::Clamp(static_cast<float>(Level) / 10.f, 0.1f, 1.f);

	VFX->SetVariableStaticMesh(FName(TEXT("User.01 - Mesh -> Weapon")), TargetMesh->GetStaticMesh());
	VFX->SetVariableLinearColor(FName(TEXT("User.03 - Color -> Emissive")), AuraColor);
	VFX->SetVariableLinearColor(FName(TEXT("User.03 - Color -> Overlay")), AuraColor * (0.55f + TierAlpha * 0.45f));
	VFX->SetVariableLinearColor(FName(TEXT("User.03 - Color -> Overlay Noise")), AuraColor);
	VFX->SetVariableFloat(FName(TEXT("User.Sparks Amount")), FMath::Lerp(18.f, 90.f, TierAlpha));
	VFX->SetVariableFloat(FName(TEXT("User.Trail Ribbon Width")), FMath::Lerp(8.f, 24.f, TierAlpha));
	VFX->SetVariableFloat(FName(TEXT("User.Trail Ribbon Lifetime")), FMath::Lerp(0.25f, 0.85f, TierAlpha));
	VFX->ComponentTags.AddUnique(FName(TEXT("Retrieve.VFX.WeaponEnhancement")));
	VFX->Activate(true);
	WeaponEnhancementVFXComponents.Add(VFX);
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
		// PartName 태그 — attachment의 OwnerComponentTag 타깃이 이 파트를 찾게 한다(예: 화살 → 활 메시).
		if (!Attachment.PartName.IsNone())
		{
			Comp->ComponentTags.Add(Attachment.PartName);
		}
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
	// PartName 태그(OwnerComponentTag 타깃용).
	if (!Attachment.PartName.IsNone())
	{
		Comp->ComponentTags.Add(Attachment.PartName);
	}
	// 메인 AnimBP 지정 시 붙인다 — 활 메시가 사격 몽타주를 재생하려면 필요(Slot 포함 ABP).
	if (TSubclassOf<UAnimInstance> AnimClass = Attachment.MeshAnimClass.LoadSynchronous())
	{
		Comp->SetAnimInstanceClass(AnimClass);
	}
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
