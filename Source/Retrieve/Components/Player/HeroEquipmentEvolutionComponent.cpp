#include "Components/Player/HeroEquipmentEvolutionComponent.h"

#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Components/Inventory/InventoryComponent.h"
#include "Components/Player/ArmorComponent.h"
#include "Components/Player/WeaponComponent.h"
#include "Data/HeroEvolutionConfig.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "Logging/RetrieveLogChannels.h"
#include "Save/RetrieveSaveSubsystem.h"
#include "TimerManager.h"

UHeroEquipmentEvolutionComponent::UHeroEquipmentEvolutionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// 전설 세트 2배 보너스 GE 기본값(BP 미지정 시 사용). DataTable 세트보너스 태그에 의존하지 않고
	// 이 컴포넌트가 전설 아이템 착용을 직접 감지해 부여한다.
	LegendaryBonusEffect = TSoftClassPtr<UGameplayEffect>(FSoftClassPath(
		TEXT("/Game/Retrieve/AbilitySystem/Player/ArmorSets/GE_Set_LegendaryHero_4pc.GE_Set_LegendaryHero_4pc_C")));
}

void UHeroEquipmentEvolutionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UninitializeFromAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

bool UHeroEquipmentEvolutionComponent::HasAuthorityToModify() const
{
	const AActor* Owner = GetOwner();
	return !Owner || Owner->HasAuthority();
}

URetrieveSaveSubsystem* UHeroEquipmentEvolutionComponent::GetSaveSubsystem() const
{
	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			return GI->GetSubsystem<URetrieveSaveSubsystem>();
		}
	}
	return nullptr;
}

void UHeroEquipmentEvolutionComponent::InitializeWithAbilitySystem(URetrieveAbilitySystemComponent* InASC)
{
	if (!InASC || !HasAuthorityToModify())
	{
		return;
	}

	// 이미 같은 ASC로 초기화됐으면 재바인딩하지 않는다.
	if (ASC.Get() == InASC && bDelegatesBound)
	{
		RecomputeSetComplete();
		return;
	}

	UninitializeFromAbilitySystem();
	ASC = InASC;

	// 어빌리티 활성화 훅(흡수/버스트 카운트). GA 파일 수정 없이 활성 이벤트만 관찰한다.
	AbilityActivatedHandle = InASC->AbilityActivatedCallbacks.AddUObject(
		this, &UHeroEquipmentEvolutionComponent::HandleAbilityActivated);

	// 장착 변화 → 세트 완성 재판정.
	if (AActor* Owner = GetOwner())
	{
		if (UWeaponComponent* Weapon = Owner->FindComponentByClass<UWeaponComponent>())
		{
			Weapon->OnWeaponEquipped.AddDynamic(this, &UHeroEquipmentEvolutionComponent::OnWeaponChanged);
			Weapon->OnWeaponUnequipped.AddDynamic(this, &UHeroEquipmentEvolutionComponent::OnWeaponChanged);
		}
		if (UArmorComponent* Armor = Owner->FindComponentByClass<UArmorComponent>())
		{
			Armor->OnArmorEquipped.AddDynamic(this, &UHeroEquipmentEvolutionComponent::OnArmorChanged);
			Armor->OnArmorUnequipped.AddDynamic(this, &UHeroEquipmentEvolutionComponent::OnArmorChanged);
		}
	}

	bDelegatesBound = true;

	// 세이브에서 진화 완료 상태면 카운트하지 않는다. 최초 세트 판정 + UI 진행도 브로드캐스트.
	RecomputeSetComplete();
	BroadcastProgress();
}

void UHeroEquipmentEvolutionComponent::UninitializeFromAbilitySystem()
{
	if (URetrieveAbilitySystemComponent* PinnedASC = ASC.Get())
	{
		if (AbilityActivatedHandle.IsValid())
		{
			PinnedASC->AbilityActivatedCallbacks.Remove(AbilityActivatedHandle);
		}
		if (LegendaryBonusHandle.IsValid())
		{
			PinnedASC->RemoveActiveGameplayEffect(LegendaryBonusHandle);
			LegendaryBonusHandle.Invalidate();
		}
	}
	AbilityActivatedHandle.Reset();

	if (AActor* Owner = GetOwner())
	{
		if (UWeaponComponent* Weapon = Owner->FindComponentByClass<UWeaponComponent>())
		{
			Weapon->OnWeaponEquipped.RemoveDynamic(this, &UHeroEquipmentEvolutionComponent::OnWeaponChanged);
			Weapon->OnWeaponUnequipped.RemoveDynamic(this, &UHeroEquipmentEvolutionComponent::OnWeaponChanged);
		}
		if (UArmorComponent* Armor = Owner->FindComponentByClass<UArmorComponent>())
		{
			Armor->OnArmorEquipped.RemoveDynamic(this, &UHeroEquipmentEvolutionComponent::OnArmorChanged);
			Armor->OnArmorUnequipped.RemoveDynamic(this, &UHeroEquipmentEvolutionComponent::OnArmorChanged);
		}
	}

	ASC = nullptr;
	bDelegatesBound = false;
}

int32 UHeroEquipmentEvolutionComponent::GetCharge() const
{
	// 진행바는 숫자 하나만 받으므로 착용 중인 잊혀진 부위 중 최대값을 대표로 쓴다.
	const URetrieveSaveSubsystem* Save = GetSaveSubsystem();
	if (!Save)
	{
		return 0;
	}

	TArray<FName> Equipped;
	CollectEquippedForgottenItems(Equipped);

	int32 Best = 0;
	for (const FName ItemId : Equipped)
	{
		Best = FMath::Max(Best, Save->GetHeroEvolutionChargeForItem(ItemId));
	}
	return Best;
}

int32 UHeroEquipmentEvolutionComponent::GetChargeForItem(FName ForgottenItemId) const
{
	const URetrieveSaveSubsystem* Save = GetSaveSubsystem();
	return Save ? Save->GetHeroEvolutionChargeForItem(ForgottenItemId) : 0;
}

int32 UHeroEquipmentEvolutionComponent::GetChargeThreshold() const
{
	return Config ? Config->ChargeThreshold : 0;
}

bool UHeroEquipmentEvolutionComponent::IsEvolved() const
{
	const URetrieveSaveSubsystem* Save = GetSaveSubsystem();
	return Save && Save->IsHeroEquipmentEvolved();
}

void UHeroEquipmentEvolutionComponent::OnWeaponChanged(FName /*WeaponItemId*/)
{
	RecomputeSetComplete();
}

void UHeroEquipmentEvolutionComponent::OnArmorChanged(FGameplayTag /*EquipmentSlotTag*/, FName /*ArmorItemId*/)
{
	RecomputeSetComplete();
}

void UHeroEquipmentEvolutionComponent::HandleAbilityActivated(UGameplayAbility* Ability)
{
	if (!Ability || !Config || bEvolving)
	{
		return;
	}
	if (!bSetComplete)
	{
		return;
	}
	if (Config->QualifyingAbilityTags.IsEmpty() ||
		!Ability->GetAssetTags().HasAny(Config->QualifyingAbilityTags))
	{
		return;
	}

	AddChargeToEquippedItems(1);
}

void UHeroEquipmentEvolutionComponent::RecomputeSetComplete()
{
	MigrateLegacyChargeIfNeeded();

	TArray<FName> Equipped;
	CollectEquippedForgottenItems(Equipped);

	const int32 Threshold = GetChargeThreshold();

	// 아직 임계치를 못 채운 착용 부위가 하나라도 있으면 충전 중 상태(= 진행바 표시).
	bool bAnyCharging = false;
	FString ChargeList;
	for (const FName ItemId : Equipped)
	{
		const int32 ItemCharge = GetChargeForItem(ItemId);
		bAnyCharging |= (ItemCharge < Threshold);
		ChargeList += FString::Printf(TEXT("%s(%d/%d) "), *ItemId.ToString(), ItemCharge, Threshold);
	}

	const bool bNow = Config && bAnyCharging;

	// 진단: 부위별 충전 상태를 한 줄로 확인한다. 장착이 바뀔 때만 찍히므로 스팸이 아니다.
	UE_LOG(LogRetrieveCombat, Verbose,
		TEXT("[HeroEvolution] SetCheck config=%d forgottenPieces=%d charging=%d charges=[%s]"),
		Config ? 1 : 0, Equipped.Num(), bNow ? 1 : 0, *ChargeList);

	if (bNow != bSetComplete)
	{
		bSetComplete = bNow;
		BroadcastProgress();
	}

	// 이미 임계치를 채운 부위를 착용했다면(다시 끼웠거나 같은 아이템을 또 주웠거나) 즉시 진화시킨다.
	ScheduleEvolutionIfReady();

	// 전설 세트(진화 후) 착용 여부에 따라 2배 보너스 GE를 적용/회수한다.
	RefreshLegendaryBonus();
}

void UHeroEquipmentEvolutionComponent::CollectEquippedForgottenItems(TArray<FName>& OutItemIds) const
{
	OutItemIds.Reset();

	AActor* Owner = GetOwner();
	if (!Owner || !Config)
	{
		return;
	}

	// 잊혀진 장비 = ItemEvolutionMap의 '키'.
	// ArmorSetTag/WeaponSetTag는 데이터 저장 이슈로 비어 있을 수 있어 진화 매핑으로 판정한다.
	if (const UWeaponComponent* Weapon = Owner->FindComponentByClass<UWeaponComponent>())
	{
		const FName WeaponId = Weapon->GetCurrentWeaponDataRow();
		if (Weapon->IsEquipped() && Config->ItemEvolutionMap.Contains(WeaponId))
		{
			OutItemIds.AddUnique(WeaponId);
		}
	}

	if (const UArmorComponent* Armor = Owner->FindComponentByClass<UArmorComponent>())
	{
		for (const FRetrieveEquippedArmorEntry& Entry : Armor->GetEquippedArmorEntries())
		{
			if (!Entry.ArmorItemId.IsNone() && Config->ItemEvolutionMap.Contains(Entry.ArmorItemId))
			{
				OutItemIds.AddUnique(Entry.ArmorItemId);
			}
		}
	}
}

void UHeroEquipmentEvolutionComponent::MigrateLegacyChargeIfNeeded()
{
	URetrieveSaveSubsystem* Save = GetSaveSubsystem();
	if (!Save || !Config || Save->HasAnyHeroEvolutionChargeEntry())
	{
		return;
	}

	const int32 LegacyCharge = Save->GetHeroEvolutionCharge();
	if (LegacyCharge <= 0)
	{
		return;
	}

	// 옛 세이브는 전역 충전 1개만 갖고 있었다. 그 진행도는 '그때 착용 중이던 부위'가 쌓은 것이므로
	// 착용 중인 잊혀진 부위에만 이관한다.
	// 잊혀진 장비는 유니크(각 1개)라, 아직 못 찾은 부위에까지 넣으면 나중에 주웠을 때
	// 충전 없이 즉시 진화해 버린다.
	TArray<FName> Equipped;
	CollectEquippedForgottenItems(Equipped);
	if (Equipped.IsEmpty())
	{
		// 세이브 로드 직후라 장비 복원이 아직 안 끝났을 수 있다.
		// 아무것도 기록하지 않고 다음 장착 변경 때 다시 시도한다.
		return;
	}

	for (const FName ItemId : Equipped)
	{
		Save->SetHeroEvolutionChargeForItem(ItemId, LegacyCharge);
	}

	UE_LOG(LogRetrieveCombat, Log,
		TEXT("[HeroEvolution] 레거시 전역 충전 %d을 착용 중인 %d개 부위로 이관"),
		LegacyCharge, Equipped.Num());
}

void UHeroEquipmentEvolutionComponent::ScheduleEvolutionIfReady()
{
	if (bEvolving || bEvolutionScheduled || !Config)
	{
		return;
	}

	TArray<FName> Equipped;
	CollectEquippedForgottenItems(Equipped);

	const int32 Threshold = GetChargeThreshold();
	const bool bAnyReady = Equipped.ContainsByPredicate(
		[this, Threshold](FName ItemId) { return GetChargeForItem(ItemId) >= Threshold; });

	if (!bAnyReady)
	{
		return;
	}

	// 진화는 어빌리티 활성화 콜백 '도중'에 트리거될 수 있다. 그 자리에서 장비를 교체하면
	// 재진입 문제가 생기므로 다음 틱으로 미룬다.
	if (UWorld* World = GetWorld())
	{
		bEvolutionScheduled = true;
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			bEvolutionScheduled = false;
			PerformEvolution();
		}));
	}
}

bool UHeroEquipmentEvolutionComponent::IsLegendarySetComplete() const
{
	AActor* Owner = GetOwner();
	if (!Owner || !Config)
	{
		return false;
	}

	const UWeaponComponent* Weapon = Owner->FindComponentByClass<UWeaponComponent>();
	const UArmorComponent* Armor = Owner->FindComponentByClass<UArmorComponent>();
	if (!Weapon || !Armor || !Weapon->IsEquipped())
	{
		return false;
	}

	// 전설 아이템 = ItemEvolutionMap의 '값'. 값 집합을 만들어 착용 아이템과 대조한다.
	TSet<FName> LegendaryIds;
	for (const TPair<FName, FName>& Pair : Config->ItemEvolutionMap)
	{
		LegendaryIds.Add(Pair.Value);
	}

	if (!LegendaryIds.Contains(Weapon->GetCurrentWeaponDataRow()))
	{
		return false;
	}

	int32 MatchingPieces = 0;
	for (const FRetrieveEquippedArmorEntry& Entry : Armor->GetEquippedArmorEntries())
	{
		if (LegendaryIds.Contains(Entry.ArmorItemId))
		{
			++MatchingPieces;
		}
	}

	return MatchingPieces >= Config->RequiredArmorPieceCount;
}

void UHeroEquipmentEvolutionComponent::RefreshLegendaryBonus()
{
	URetrieveAbilitySystemComponent* PinnedASC = ASC.Get();
	if (!PinnedASC || !Config)
	{
		return;
	}

	const bool bWant = IsLegendarySetComplete();
	const bool bHave = LegendaryBonusHandle.IsValid();

	if (bWant && !bHave)
	{
		UClass* EffectClass = LegendaryBonusEffect.LoadSynchronous();
		if (EffectClass)
		{
			FGameplayEffectContextHandle Context = PinnedASC->MakeEffectContext();
			Context.AddSourceObject(this);
			const FGameplayEffectSpecHandle SpecHandle = PinnedASC->MakeOutgoingSpec(EffectClass, 1.f, Context);
			if (SpecHandle.IsValid())
			{
				LegendaryBonusHandle = PinnedASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
				UE_LOG(LogRetrieveCombat, Log, TEXT("[HeroEvolution] 전설 세트 보너스(2배) 적용"));
			}
		}
	}
	else if (!bWant && bHave)
	{
		PinnedASC->RemoveActiveGameplayEffect(LegendaryBonusHandle);
		LegendaryBonusHandle.Invalidate();
		UE_LOG(LogRetrieveCombat, Log, TEXT("[HeroEvolution] 전설 세트 보너스(2배) 회수"));
	}
}

void UHeroEquipmentEvolutionComponent::AddChargeToEquippedItems(int32 Delta)
{
	URetrieveSaveSubsystem* Save = GetSaveSubsystem();
	if (!Save || !Config)
	{
		return;
	}

	TArray<FName> Equipped;
	CollectEquippedForgottenItems(Equipped);
	if (Equipped.IsEmpty())
	{
		return;
	}

	// 착용 중인 잊혀진 부위 '각각'이 자기 충전을 쌓는다.
	// 세트로 끼고 다니면 같은 속도로 오르고, 늦게 주운 부위만 뒤처진다.
	for (const FName ItemId : Equipped)
	{
		const int32 NewCharge = FMath::Clamp(
			Save->GetHeroEvolutionChargeForItem(ItemId) + Delta, 0, Config->ChargeThreshold);
		Save->SetHeroEvolutionChargeForItem(ItemId, NewCharge);

		UE_LOG(LogRetrieveCombat, Verbose, TEXT("[HeroEvolution] Charge %s %d/%d"),
			*ItemId.ToString(), NewCharge, Config->ChargeThreshold);
	}

	BroadcastProgress();
	ScheduleEvolutionIfReady();
}

void UHeroEquipmentEvolutionComponent::PerformEvolution()
{
	if (bEvolving || !Config || !HasAuthorityToModify())
	{
		return;
	}

	AActor* Owner = GetOwner();
	UInventoryComponent* Inv = Owner ? Owner->FindComponentByClass<UInventoryComponent>() : nullptr;
	UWeaponComponent* Weapon = Owner ? Owner->FindComponentByClass<UWeaponComponent>() : nullptr;
	UArmorComponent* Armor = Owner ? Owner->FindComponentByClass<UArmorComponent>() : nullptr;
	if (!Inv || !Weapon || !Armor)
	{
		return;
	}

	bEvolving = true;
	int32 EvolvedCount = 0;

	// 교체는 InventoryComponent::ReplaceEquipped*로만 한다.
	// 컴포넌트(WeaponComponent/ArmorComponent)를 직접 호출해 장착하면 인벤토리의 장착 기록이
	// 여전히 '잊혀진' 아이템을 가리키고, 그 상태로 잊혀진 아이템을 RemoveItem하면
	// 인벤토리가 그 슬롯을 통째로 벗겨버려 방금 장착한 전설 장비까지 같이 벗겨진다.
	// Replace 계열은 기록 갱신을 제거보다 먼저 하고, 전투 게이트도 요구하지 않는다.

	const int32 Threshold = Config->ChargeThreshold;

	// 1) 무기 스왑 — 잊혀진 무기를 착용 중이고, 그 무기의 충전이 다 찼을 때만.
	const FName ForgottenWeaponId = Weapon->GetCurrentWeaponDataRow();
	if (Weapon->IsEquipped() && GetChargeForItem(ForgottenWeaponId) >= Threshold)
	{
		const FName LegendaryWeaponId = Config->GetEvolvedItemId(ForgottenWeaponId);
		if (!LegendaryWeaponId.IsNone() &&
			Inv->ReplaceEquippedWeapon(ForgottenWeaponId, LegendaryWeaponId, Config->WeaponCategoryTag))
		{
			++EvolvedCount;
			UE_LOG(LogRetrieveCombat, Log, TEXT("[HeroEvolution] 진화 — %s → %s"),
				*ForgottenWeaponId.ToString(), *LegendaryWeaponId.ToString());
		}
	}

	// 2) 방어구 스왑: 반복 중 장착 배열이 바뀌므로 스냅샷을 떠서 순회한다.
	//    충전이 다 찬 부위만 교체한다 — 덜 찬 부위는 그대로 두고 계속 쌓는다.
	const TArray<FRetrieveEquippedArmorEntry> Entries = Armor->GetEquippedArmorEntries();
	for (const FRetrieveEquippedArmorEntry& Entry : Entries)
	{
		if (Entry.ArmorItemId.IsNone() || !Config->ItemEvolutionMap.Contains(Entry.ArmorItemId))
		{
			continue;
		}
		if (GetChargeForItem(Entry.ArmorItemId) < Threshold)
		{
			continue;
		}
		const FName LegendaryArmorId = Config->GetEvolvedItemId(Entry.ArmorItemId);
		if (LegendaryArmorId.IsNone())
		{
			continue;
		}
		// 전설 아이템의 슬롯은 잊혀진 아이템과 동일하므로 Entry.EquipmentSlotTag로 장착된다.
		if (Inv->ReplaceEquippedArmor(Entry.EquipmentSlotTag, Entry.ArmorItemId, LegendaryArmorId, Config->ArmorCategoryTag))
		{
			++EvolvedCount;
			UE_LOG(LogRetrieveCombat, Log, TEXT("[HeroEvolution] 진화 — %s → %s"),
				*Entry.ArmorItemId.ToString(), *LegendaryArmorId.ToString());
		}
	}

	// 3) 영속: 한 부위라도 진화했으면 기록을 남긴다(연출/기록용, 진화를 막는 게이트는 아니다).
	if (EvolvedCount > 0)
	{
		if (URetrieveSaveSubsystem* Save = GetSaveSubsystem())
		{
			Save->SetHeroEquipmentEvolved(true);
		}
	}

	// 말미 재판정이 다시 예약을 걸지 않도록 bEvolving을 유지한 채 호출한다
	// (교체가 실패한 부위가 남아 있으면 매 틱 재시도하는 루프가 된다).
	RecomputeSetComplete();
	BroadcastProgress();

	bEvolving = false;

	// 실제로 바뀐 게 있을 때만 연출을 띄운다.
	if (EvolvedCount > 0)
	{
		OnEvolutionCompleted.Broadcast();
	}
}

void UHeroEquipmentEvolutionComponent::BroadcastProgress()
{
	const int32 Charge = GetCharge();
	const int32 Threshold = GetChargeThreshold();
	OnEvolutionProgress.Broadcast(Charge, Threshold, bSetComplete);
}
