#include "Components/Player/WeaponComponent.h"

#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Components/Element/ElementUnlockComponent.h"
#include "Components/Pawn/RetrievePawnExtensionComponent.h"
#include "Components/SceneComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
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
	ElementEmpowerVFX.Add(RetrieveGameplayTags::Element_Fire,
		TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Retrieve/VFX/Weapons/ElementEmpower/NS_WeaponElementEmpower_Fire.NS_WeaponElementEmpower_Fire"))));
	ElementEmpowerVFX.Add(RetrieveGameplayTags::Element_Water,
		TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Retrieve/VFX/Weapons/ElementEmpower/NS_WeaponElementEmpower_Water.NS_WeaponElementEmpower_Water"))));
	ElementEmpowerVFX.Add(RetrieveGameplayTags::Element_Wind,
		TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Retrieve/VFX/Weapons/ElementEmpower/NS_WeaponElementEmpower_Wind.NS_WeaponElementEmpower_Wind"))));
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

	ClearWeaponData(); // ?댁쟾 臾닿린 '?곗씠?곕쭔' ?뺣━ (OLD 硫붿떆??Pending?쇰줈 ?섍꺼 援먯껜 ?곗텧???앸궪 ???뚭눼)
	if (!ApplyWeaponData(WeaponItemId, *WeaponData))
	{
		return false;
	}

	// OLD 硫붿떆瑜?NEW ?ㅽ룿怨?遺꾨━??Pending?쇰줈 ??릿?? 利됱떆 ?④꺼 NEW(?명떚濡??깆옣)? 寃뱀튂吏 ?딄쾶 ?섍퀬,
	// ?ㅼ젣 ?뚭눼??援먯껜 紐쏀?二????먮뒗 fallback 利됱떆)?쇰줈 誘몃，??
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

	// NEW ?덉씠?대줈 癒쇱? relink ??GA媛 NEW EquipMontage瑜??쎈룄濡?broadcast瑜??몃━嫄곕낫???욎뿉 ?붾떎.
	OnWeaponEquipped.Broadcast(CurrentWeaponDataRow);

	// ?곗텧/鍮꾩＜?쇱? GA媛. ?몃━嫄??ㅽ뙣(誘몃???紐쏀?二??놁쓬)硫?OLD 利됱떆 ?뚭눼 + ?곗씠?곕줈 由щ퉴??
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

	// ?댁젣 ?곗텧? '踰쀫뒗' 臾닿린(=?꾩옱 留곹겕???덉씠????UnequipMontage.
	// relink(Unarmed) ?꾩뿉 ?몃━嫄고빐 ?덉씠?닿? ?댁븘?덉쓣 ??李몄“瑜??↔쾶 ?쒕떎.
	const bool bTriggered = TryTriggerEquipTransition(RetrieveGameplayTags::GameplayEvent_Player_UnequipWeapon);

	OnWeaponUnequipped.Broadcast(PreviousWeaponId); // ??Cosmetic??Unarmed濡?relink

	if (!bTriggered)
	{
		ReconcileVisuals();
	}
}

void UWeaponComponent::ClearWeaponData()
{
	// 臾닿린 怨듦꺽??GE 癒쇱? ?쒓굅 (ClearGrantedWeaponAbilities ?꾩뿉 ?섑뻾)
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
		// ???뚯폆???ㅽ룿. ?⑷? ?곹깭 蹂댁젙? ?ㅽ룿 吏곹썑 OnWeaponVisualsSpawned??諛쏅뒗 CombatStance媛
		// SetWeaponDrawn?쇰줈 泥섎━?쒕떎(???좏샇??紐⑤뱺 ?ㅽ룿 寃쎈줈???⑥씪 ?듬줈 ??fallback/OnRep/?μ갑 ?명떚 怨듯넻).
		// Equip ?꾪솚 以묒씠硫??④꺼???ㅽ룿 ??諛쒓? ?명떚媛 ?먯뿉 遺李⑺븯硫?蹂댁씠寃??쒕떎(寃?믩갑???쒖감 ?깆옣).
		ApplyWeaponVisuals(CurrentWeaponData, /*bSpawnHidden=*/IsEquipTransitionActive());
		SpawnWeaponEnhancementVFX();
		RefreshElementEmpowerVFX();
		OnWeaponVisualsSpawned.Broadcast();
		// ?덈줈 ?ㅽ룿??寃???꾩옱 ?먯냼紐⑤뱶 癒명떚由ъ뼹??利됱떆 諛섏쁺(臾닿린 援먯껜쨌?ъ옣李????.
		ApplyElementModeMaterial();
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
		// Equip ?꾨즺 ??draw ?명떚媛 釉붾젋?쒕줈 ?꾨씫?먯쓣 ???덉쑝?????뚰듃瑜?蹂댁씠寃?媛뺤젣(???뚯폆???ㅽ룿???덉쓬).
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
		// Unequip ?꾨즺 ???뚭눼 ?명떚媛 ?꾨씫?먯쓣 ???덉쑝??硫붿떆 ?뺣━ 蹂댁옣.
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

	// ?대씪?댁뼵?몃뒗 蹂듭젣??RowName 湲곗??쇰줈 鍮꾩＜?쇨낵 UI??罹먯떆留?媛깆떊
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
		ApplyWeaponData(ReplicatedWeaponId, *WeaponData); // ?대씪: 罹먯떆留?(?대퉴由ы떚/GE??沅뚯쐞 媛??
		OnWeaponEquipped.Broadcast(ReplicatedWeaponId);   // ??Cosmetic relink
		ReconcileVisuals();                               // ?먭꺽 利됱떆 ?ㅽ룿 (?곗텧 ?앸왂)
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

void UWeaponComponent::InitializeWithAbilitySystem(URetrieveAbilitySystemComponent* InASC)
{
	if (!InASC)
	{
		return;
	}

	// ?숈씪 ASC ?ъ큹湲고솕硫??щ컮?몃뵫 ?놁씠 ?꾩옱 ?곹깭留?諛섏쁺
	if (ElementEventASC.Get() == InASC && ElementModeChangedHandle.IsValid())
	{
		ApplyElementModeMaterial();
		RefreshElementEmpowerVFX();
		return;
	}

	UninitializeFromAbilitySystem();
	ElementEventASC = InASC;

	ElementModeChangedHandle = InASC->GenericGameplayEventCallbacks
		.FindOrAdd(RetrieveGameplayTags::GameplayEvent_Element_ModeChange)
		.AddUObject(this, &UWeaponComponent::OnElementModeChanged);

	// ?꾩옱 ?먯냼紐⑤뱶瑜?ASC ?쒓렇濡?珥덇린???μ갑 ?좏뻾 ??利됱떆 ?щ컮瑜???諛섏쁺)
	if (InASC->HasMatchingGameplayTag(RetrieveGameplayTags::Element_Fire))
	{
		CurrentElementModeTag = RetrieveGameplayTags::Element_Fire;
	}
	else if (InASC->HasMatchingGameplayTag(RetrieveGameplayTags::Element_Water))
	{
		CurrentElementModeTag = RetrieveGameplayTags::Element_Water;
	}
	else if (InASC->HasMatchingGameplayTag(RetrieveGameplayTags::Element_Wind))
	{
		CurrentElementModeTag = RetrieveGameplayTags::Element_Wind;
	}
	else
	{
		CurrentElementModeTag = FGameplayTag();
	}

	ApplyElementModeMaterial();

	// ?먯냼 ?대갑 ?곹깭 援щ룆. ElementUnlockComponent媛 Weapon蹂대떎 癒쇱? init?섎?濡?SovereignCharacter)
	// ?몄씠釉?蹂듭썝???대갑 紐⑸줉?????쒖젏 Refresh?먯꽌 諛붾줈 ?쎌쓣 ???덈떎.
	if (const AActor* Owner = GetOwner())
	{
		if (UElementUnlockComponent* Unlock = Owner->FindComponentByClass<UElementUnlockComponent>())
		{
			CachedElementUnlockComponent = Unlock;
			Unlock->OnElementUnlocked.AddUniqueDynamic(this, &UWeaponComponent::HandleElementUnlockedForVFX);
		}
	}
	RefreshElementEmpowerVFX();
}

void UWeaponComponent::UninitializeFromAbilitySystem()
{
	if (UElementUnlockComponent* Unlock = CachedElementUnlockComponent.Get())
	{
		Unlock->OnElementUnlocked.RemoveDynamic(this, &UWeaponComponent::HandleElementUnlockedForVFX);
	}
	CachedElementUnlockComponent = nullptr;

	if (URetrieveAbilitySystemComponent* ASC = ElementEventASC.Get())
	{
		if (ElementModeChangedHandle.IsValid())
		{
			if (FGameplayEventMulticastDelegate* Delegate =
				ASC->GenericGameplayEventCallbacks.Find(RetrieveGameplayTags::GameplayEvent_Element_ModeChange))
			{
				Delegate->Remove(ElementModeChangedHandle);
			}
		}
	}
	ElementModeChangedHandle.Reset();
	ElementEventASC = nullptr;
}

void UWeaponComponent::OnElementModeChanged(const FGameplayEventData* Payload)
{
	if (Payload && !Payload->InstigatorTags.IsEmpty())
	{
		TArray<FGameplayTag> Tags;
		Payload->InstigatorTags.GetGameplayTagArray(Tags);
		CurrentElementModeTag = Tags.Num() > 0 ? Tags[0] : FGameplayTag();
	}
	else
	{
		CurrentElementModeTag = FGameplayTag();
	}

	ApplyElementModeMaterial();
	RefreshElementEmpowerVFX();
}

void UWeaponComponent::ApplyElementModeMaterial()
{
	if (!IsEquipped() || !CurrentElementModeTag.IsValid())
	{
		return;
	}

	const TMap<FGameplayTag, TSoftObjectPtr<UMaterialInterface>>& ElementMats = CurrentWeaponData.ElementModeMaterials;
	if (ElementMats.Num() == 0)
	{
		return; // 湲곕낯 臾닿린(?먯냼蹂?癒명떚由ъ뼹 誘몄??? ???꾨Т寃껊룄 ?섏? ?딆쓬
	}

	const TSoftObjectPtr<UMaterialInterface>* MatPtr = ElementMats.Find(CurrentElementModeTag);
	if (!MatPtr)
	{
		return;
	}

	UMaterialInterface* Material = MatPtr->LoadSynchronous();
	if (!Material)
	{
		return;
	}

	// 寃(二?臾닿린 硫붿떆)??紐⑤뱺 癒명떚由ъ뼹 ?щ’???곸슜.
	UMeshComponent* SwordMesh = GetPrimaryEquippedWeaponMesh();
	if (!IsValid(SwordMesh))
	{
		return;
	}

	// 레이어 분리: 원소 모드는 무기 몸체 머티리얼을 덮지 않는다.
	// (기존: 원소별 머티리얼이 몸체를 덮어 강화 오라 색을 가리고, 복원 로직이 없어
	//  다른 원소 모드로 전환해도 잔상이 남던 문제 — 예: 불 모드인데 이전 바람의 초록이 유지)
	// 무기 몸체색은 무기 고유색 + 강화 오라(SpawnWeaponEnhancementVFX)가 소유하고,
	// 원소 해방은 empower VFX(스파크/외곽 아우라)로만 표현한다.
	(void)SwordMesh;
	(void)Material;
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
	// ?쒕쾭?먯꽌 遺?ы븳 臾닿린 ?꾩슜 ?대퉴由ы떚留??뚯닔
	if (URetrieveAbilitySystemComponent* ASC = GetRetrieveAbilitySystemComponent())
	{
		WeaponGrantedHandles.TakeFromAbilitySystem(ASC);
	}
}

void UWeaponComponent::ClearWeaponVisuals()
{
	ClearWeaponEnhancementVFX();
	ClearElementEmpowerVFX();
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
		// AbilitySet 遺?ъ? 臾닿린 怨듦꺽??GE ?곸슜? ?쒕쾭?먯꽌留?泥섎━
		// ?대씪?댁뼵??OnRep 寃쎈줈??鍮꾩＜?쇰쭔 媛깆떊
		if (URetrieveAbilitySystemComponent* ASC = GetRetrieveAbilitySystemComponent())
		{
			if (URetrieveAbilitySet* AbilitySet = Cast<URetrieveAbilitySet>(WeaponData.WeaponAbilitySet.TryLoad()))
			{
				AbilitySet->GiveToAbilitySystem(ASC, &WeaponGrantedHandles, GetOwner());
			}

			// 臾닿린 AttackPower瑜?罹먮┃???댄듃由щ럭?몄뿉 媛??
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

	// 鍮꾩＜???ㅽ룿怨?OnWeaponEquipped 釉뚮줈?쒖틦?ㅽ듃???몄텧??EquipWeapon / OnRep)媛 ?대떦?쒕떎.
	// (援먯껜 ?곗텧 ??대컢 ?쒖뼱 + relink ?쒖꽌 蹂댁옣???꾪빐 ?곗씠???곸슜怨?遺꾨━)
	return true;
}

bool UWeaponComponent::ApplyWeaponVisuals(const FRetrieveWeaponDataRow& WeaponData, bool bSpawnHidden)
{
	if (WeaponData.Attachments.IsEmpty())
	{
		return false;
	}

	// ???섎궔) ?뚯폆???ㅽ룿 ?쒖젏??臾닿린 ??낆쑝濡?1踰??댁꽍???뚰듃??罹먯떛?쒕떎.
	// (?댄썑 SetWeaponDrawn? ?뚰듃媛믩쭔 ?곕?濡?Unequip?쇰줈 ????쒓렇媛 吏?뚯졇???⑷? ?꾩튂媛 ?좎??쒕떎)
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

		// 諛쒓?/?⑷? ?뚯폆 ?ㅼ솑??湲곕줉(??AttachSocketName, ?ㅽ봽??蹂댁〈). ???뚯폆? SetWeaponDrawn?먯꽌 ?덉씠??留듭쑝濡??댁꽍.
		FRetrieveEquippedWeaponPart& Part = WeaponAttachParts.AddDefaulted_GetRef();
		Part.Mesh = WeaponMeshComponent;
		Part.DrawnSocket = Attachment.AttachSocketName;
		Part.SheathedSocket = SheathedMap ? SheathedMap->DrawnToSheathed.FindRef(Attachment.AttachSocketName) : NAME_None;
		Part.RelativeTransform = Attachment.RelativeTransform;

		// ?명궧 ?붿궡 ?ㅽ룿 媛?쒖꽦? ?곗씠?곕줈: 臾댄븳(??긽 ?명궧)=visible, ?좏븳=hidden(Reload ?명떚媛 ?쒖떆).
		if (Attachment.bIsNockedArrow)
		{
			WeaponMeshComponent->SetVisibility(Attachment.bNockedArrowStartsVisible, /*bPropagateToChildren=*/true);
			NockedArrowMeshes.Add(WeaponMeshComponent);
			if (Attachment.bNockedArrowStartsVisible)
			{
				bArrowNocked = true; // ?ㅽ룿遺???명궧 ??GA媛 ?μ쟾 ?ㅽ궢(臾댄븳 ??
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
		// ?뱀젙 ?뚰듃留?吏?뺣맂 寃쎌슦(寃/諛⑺뙣 ??대컢 遺꾨━) ?섎㉧吏??嫄대꼫?대떎. None?대㈃ ?꾩껜 泥섎━.
		if (!OnlyDrawnSocket.IsNone() && Part.DrawnSocket != OnlyDrawnSocket)
		{
			continue;
		}

		UMeshComponent* Mesh = Part.Mesh;
		if (!IsValid(Mesh))
		{
			continue;
		}

		// ?④? 吏?????뚯폆 ?녿뒗 諛⑺뙣 ??: ?뚯폆 ?ㅼ솑 ?놁씠 Hidden 泥섎━. 怨?ClearWeapon???뚭눼?쒕떎.
		if (bSetHidden)
		{
			Mesh->SetVisibility(false, /*bPropagateToChildren=*/true);
			continue;
		}

		// ???몄텧????곹븳 ?뚰듃??蹂댁씠寃??쒕떎(Equip 以?hidden ?ㅽ룿 ??諛쒓? ?명떚媛 ?ш린???깆옣).
		Mesh->SetVisibility(true, /*bPropagateToChildren=*/true);

		// ?⑷? ?뚯폆 留ㅽ븨???녿뒗 ?뚰듃??洹몃?濡??붾떎(?? ??긽 ?먯뿉 ?덈뒗 臾닿린 ??.
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
		Mesh->SetRelativeTransform(Part.RelativeTransform); // SnapToTarget??0?쇰줈 留뚮뱺 ?ㅽ봽??蹂듭썝
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

UStaticMeshComponent* UWeaponComponent::FindWeaponVFXTargetMesh() const
{
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
	return TargetMesh; // ?먮낯 Aura ?쒖뒪?쒖씠 StaticMesh ?낅젰?뺤씠???ㅼ펷?덊깉 臾닿린???쒖쇅?쒕떎.
}

void UWeaponComponent::SpawnWeaponEnhancementVFX()
{
	ClearWeaponEnhancementVFX();

	if (CurrentWeaponData.EnhancementLevel <= 0)
	{
		return;
	}

	UStaticMeshComponent* TargetMesh = FindWeaponVFXTargetMesh();
	if (!TargetMesh)
	{
		return;
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

	// 강화 오라 색을 원소(어피니티)와 분리 — 무기 타입 기준으로 결정한다.
	const FLinearColor AuraColor = CurrentWeaponTypeTag == RetrieveGameplayTags::Weapon_Type_Staff
		? FLinearColor(0.02f, 0.8f, 3.0f, 1.f)      // 스태프 = 청색
		: CurrentWeaponTypeTag == RetrieveGameplayTags::Weapon_Type_SwordShield
			? FLinearColor(1.2f, 0.4f, 2.5f, 1.f)       // 검·방패 = 검보라
			: CurrentWeaponTypeTag == RetrieveGameplayTags::Weapon_Type_Bow
				? FLinearColor(0.08f, 2.2f, 0.5f, 1.f)  // 활 = 에메랄드
				: FLinearColor(1.2f, 0.4f, 2.5f, 1.f);  // 기타(쌍검/기본) = 검보라
	// 媛뺥솕 ?덈꺼(1~10)??0~1濡??뺢퇋?? ?④퀎蹂?李⑥씠瑜??볤쾶 踰뚮━湲??꾪빐 理쒖?(?덈꺼1)??0??媛源앷쾶 ?붾떎.
	const float TierAlpha = FMath::Clamp((static_cast<float>(Level) - 1.f) / 9.f, 0.f, 1.f);

	// 媛뺥솕 ?④퀎媛 ?뺤떎??泥닿컧?섎룄濡?諛쒓킅/?ㅻ쾭?덉씠 諛앷린 ?먯껜瑜??덈꺼??鍮꾨??쒗궓??
	// 理쒓퀬 ?덈꺼(10) = ?꾩옱 ?쒕떇??湲곗? 諛앷린(AuraColor), ??? ?덈꺼? ???섍쾶.
	const float LevelIntensity = FMath::Lerp(0.5f, 1.0f, TierAlpha);
	const FLinearColor ScaledColor = AuraColor * LevelIntensity * 0.3f; // 발광 밝기 하향(0.3)

	VFX->SetVariableStaticMesh(FName(TEXT("User.01 - Mesh -> Weapon")), TargetMesh->GetStaticMesh());
	VFX->SetVariableLinearColor(FName(TEXT("User.03 - Color -> Emissive")), ScaledColor);
	VFX->SetVariableLinearColor(FName(TEXT("User.03 - Color -> Overlay")), ScaledColor * (0.5f + TierAlpha * 0.5f));
	VFX->SetVariableLinearColor(FName(TEXT("User.03 - Color -> Overlay Noise")), ScaledColor);
	VFX->SetVariableFloat(FName(TEXT("User.Sparks Amount")), FMath::Lerp(20.f, 110.f, TierAlpha));
	VFX->SetVariableFloat(FName(TEXT("User.Trail Ribbon Width")), FMath::Lerp(5.f, 26.f, TierAlpha));
	VFX->SetVariableFloat(FName(TEXT("User.Trail Ribbon Lifetime")), FMath::Lerp(0.15f, 0.85f, TierAlpha));
	VFX->ComponentTags.AddUnique(FName(TEXT("Retrieve.VFX.WeaponEnhancement")));
	VFX->Activate(true);
	WeaponEnhancementVFXComponents.Add(VFX);
}

void UWeaponComponent::ClearElementEmpowerVFX()
{
	for (UNiagaraComponent* Component : ElementEmpowerVFXComponents)
	{
		if (IsValid(Component))
		{
			Component->DeactivateImmediate();
			Component->DestroyComponent();
		}
	}
	ElementEmpowerVFXComponents.Reset();
}

void UWeaponComponent::RefreshElementEmpowerVFX()
{
	ClearElementEmpowerVFX();

	if (!IsEquipped() || !CurrentElementModeTag.IsValid())
	{
		return;
	}

	// ???쒖쇅 ???먯냼 ?ㅻ씪??洹쇱젒(寃瑜? ?꾩슜 ?곗텧.
	if (CurrentWeaponTypeTag.MatchesTagExact(RetrieveGameplayTags::Weapon_Type_Bow))
	{
		return;
	}

	// 媛?붿뼵 肄붿뼱 ?≪닔濡??꾩옱 紐⑤뱶 ?먯냼媛 ?대갑(媛뺥솕)???곹깭?먯꽌留??쒖떆.
	const UElementUnlockComponent* Unlock = CachedElementUnlockComponent.Get();
	if (!Unlock || !Unlock->IsElementUnlocked(CurrentElementModeTag))
	{
		return;
	}

	const TSoftObjectPtr<UNiagaraSystem>* SystemPtr = ElementEmpowerVFX.Find(CurrentElementModeTag);
	if (!SystemPtr)
	{
		return;
	}
	UNiagaraSystem* System = SystemPtr->LoadSynchronous();
	if (!System)
	{
		return;
	}

	UStaticMeshComponent* TargetMesh = FindWeaponVFXTargetMesh();
	if (!TargetMesh)
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

	VFX->SetVariableStaticMesh(FName(TEXT("User.01 - Mesh -> Weapon")), TargetMesh->GetStaticMesh());

	// ?ㅽ뙆???ㅽ룿 ?ㅻ┛?붾? 臾닿린 濡쒖뺄 諛붿슫利덉뿉 留욎텣??寃/諛⑺뙣/吏?≪씠 湲몄씠쨌?먭퍡 ?먮룞 ???.
	// Midpoint 0.5 = ?ㅻ┛?붽? Offset Z 湲곗? ?곹븯 ?移???硫붿떆 以묒떖???볦쑝硫?臾닿린 ?꾩껜瑜???뒗??
	if (const UStaticMesh* StaticMesh = TargetMesh->GetStaticMesh())
	{
		const FBoxSphereBounds Bounds = StaticMesh->GetBounds();
		// 諛섍꼍? ?뉗? 異?移쇰궇 ?먭퍡) 湲곗? + ?쎄컙??蹂쇰ⅷ ??媛????max) 湲곗??대㈃ 遺덇만??移쇰궇?먯꽌 ?좎꽌 ?⑹뼱??蹂댁씤??
		const float WeaponRadius =
			FMath::Clamp(static_cast<float>(FMath::Min(Bounds.BoxExtent.X, Bounds.BoxExtent.Y)) + 4.f, 6.f, 10.f);
		VFX->SetVariableFloat(FName(TEXT("User.Sparks Radius")), WeaponRadius);
        // The weapon mesh pivot is attached to the hand socket. Emit from that pivot toward
        // the farther Z end instead of emitting symmetrically around the mesh center.
        const float PositiveLength = FMath::Max(0.f, static_cast<float>(Bounds.Origin.Z + Bounds.BoxExtent.Z));
        const float NegativeLength = FMath::Max(0.f, static_cast<float>(-(Bounds.Origin.Z - Bounds.BoxExtent.Z)));
        const bool bExtendsTowardPositiveZ = PositiveLength >= NegativeLength;
        const float HandToTipLength = FMath::Max(PositiveLength, NegativeLength);

        VFX->SetVariableFloat(FName(TEXT("User.Sparks Height")), HandToTipLength);
        VFX->SetVariableFloat(FName(TEXT("User.Sparks Midpoint")), bExtendsTowardPositiveZ ? 0.f : 1.f);
        VFX->SetVariableFloat(FName(TEXT("User.08 - Offset Z - Sparks")), 0.f);
        VFX->SetVariableFloat(FName(TEXT("User.08 - Offset Z - Trail/Smoke")), 0.f);

        if (CurrentElementModeTag.MatchesTagExact(RetrieveGameplayTags::Element_Wind))
        {
            // Wind helix follows the mesh's longest local axis, regardless of import orientation.
            FVector DrillAxis = FVector::ZeroVector;
            float DrillLength = 0.f;
            int32 DrillAxisIndex = 0;
            for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
            {
                const float PositiveAxisLength = FMath::Max(0.f, static_cast<float>(Bounds.Origin[AxisIndex] + Bounds.BoxExtent[AxisIndex]));
                const float NegativeAxisLength = FMath::Max(0.f, static_cast<float>(-(Bounds.Origin[AxisIndex] - Bounds.BoxExtent[AxisIndex])));
                const float CandidateLength = FMath::Max(PositiveAxisLength, NegativeAxisLength);
                if (CandidateLength > DrillLength)
                {
                    DrillLength = CandidateLength;
                    DrillAxisIndex = AxisIndex;
                    DrillAxis = FVector::ZeroVector;
                    DrillAxis[AxisIndex] = PositiveAxisLength >= NegativeAxisLength ? 1.f : -1.f;
                }
            }

            const int32 TransverseAxisA = (DrillAxisIndex + 1) % 3;
            const int32 TransverseAxisB = (DrillAxisIndex + 2) % 3;
            const float DrillWeaponRadius = FMath::Clamp(
                static_cast<float>(FMath::Min(Bounds.BoxExtent[TransverseAxisA], Bounds.BoxExtent[TransverseAxisB])) + 4.f,
                6.f, 12.f);

            VFX->SetVariableVec3(FName(TEXT("User.WindDrillAxis")), DrillAxis);
            VFX->SetVariableFloat(FName(TEXT("User.WindDrillHeight")), DrillLength);
            VFX->SetVariableFloat(FName(TEXT("User.WindDrillBaseRadius")), FMath::Max(DrillWeaponRadius * 1.65f, 12.f));
            VFX->SetVariableFloat(FName(TEXT("User.WindDrillTipRadius")), 0.8f);
            VFX->SetVariableFloat(FName(TEXT("User.WindDrillTurns")), FMath::Clamp(DrillLength / 32.f, 3.5f, 5.25f));
            VFX->SetVariableFloat(FName(TEXT("User.WindDrillTaperPower")), 1.15f);
            VFX->SetVariableFloat(FName(TEXT("User.Trail Ribbon Width")), 4.5f);
            VFX->SetVariableFloat(FName(TEXT("User.Trail Ribbon Lifetime")), 0.75f);
        }
	}

	VFX->ComponentTags.AddUnique(FName(TEXT("Retrieve.VFX.WeaponElementEmpower")));
	VFX->Activate(true);
	ElementEmpowerVFXComponents.Add(VFX);
}

void UWeaponComponent::HandleElementUnlockedForVFX(FGameplayTag /*ElementTag*/)
{
	RefreshElementEmpowerVFX();
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
		// PartName ?쒓렇 ??attachment??OwnerComponentTag ?源껋씠 ???뚰듃瑜?李얘쾶 ?쒕떎(?? ?붿궡 ????硫붿떆).
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
	// PartName ?쒓렇(OwnerComponentTag ?源껋슜).
	if (!Attachment.PartName.IsNone())
	{
		Comp->ComponentTags.Add(Attachment.PartName);
	}
	// 硫붿씤 AnimBP 吏????遺숈씤??????硫붿떆媛 ?ш꺽 紐쏀?二쇰? ?ъ깮?섎젮硫??꾩슂(Slot ?ы븿 ABP).
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

	// ?⑥씪 硫붿떆 援ъ“: 臾닿린????긽 leader ?ㅼ펷?덊넠(GetMesh)???뚯폆???뺤젙 遺李⑺븳??
	// 紐⑤뱢???뚯툩??媛숈? ?ㅼ펷?덊넠??LeaderPose濡?怨듭쑀?섎?濡??뚯툩瑜?怨좊Ⅴ硫??뚯폆??以묐났 留ㅼ묶?쒕떎.
	return HasSocket(CharacterOwner->GetMesh()) ? CharacterOwner->GetMesh() : nullptr;
}
