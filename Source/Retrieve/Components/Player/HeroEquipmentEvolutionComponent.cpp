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
	const URetrieveSaveSubsystem* Save = GetSaveSubsystem();
	return Save ? Save->GetHeroEvolutionCharge() : 0;
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
	if (IsEvolved() || !bSetComplete)
	{
		return;
	}
	if (Config->QualifyingAbilityTags.IsEmpty() ||
		!Ability->GetAssetTags().HasAny(Config->QualifyingAbilityTags))
	{
		return;
	}

	AddCharge(1);
}

void UHeroEquipmentEvolutionComponent::RecomputeSetComplete()
{
	const bool bNow = Config && !IsEvolved() && IsForgottenSetComplete();
	if (bNow != bSetComplete)
	{
		bSetComplete = bNow;
		BroadcastProgress();
	}

	// 전설 세트(진화 후) 착용 여부에 따라 2배 보너스 GE를 적용/회수한다.
	RefreshLegendaryBonus();
}

bool UHeroEquipmentEvolutionComponent::IsForgottenSetComplete() const
{
	AActor* Owner = GetOwner();
	if (!Owner || !Config)
	{
		return false;
	}

	const UWeaponComponent* Weapon = Owner->FindComponentByClass<UWeaponComponent>();
	const UArmorComponent* Armor = Owner->FindComponentByClass<UArmorComponent>();
	if (!Weapon || !Armor)
	{
		return false;
	}

	// 무기: 잊혀진 무기(=ItemEvolutionMap의 '키') 장착 중이어야 한다.
	//  ArmorSetTag/WeaponSetTag는 데이터 저장 이슈로 비어 있을 수 있어, 진화 매핑으로 판정한다.
	if (!Weapon->IsEquipped() || !Config->ItemEvolutionMap.Contains(Weapon->GetCurrentWeaponDataRow()))
	{
		return false;
	}

	// 방어구: ItemEvolutionMap의 '키'인 착용 부위 수 집계.
	int32 MatchingPieces = 0;
	for (const FRetrieveEquippedArmorEntry& Entry : Armor->GetEquippedArmorEntries())
	{
		if (!Entry.ArmorItemId.IsNone() && Config->ItemEvolutionMap.Contains(Entry.ArmorItemId))
		{
			++MatchingPieces;
		}
	}

	return MatchingPieces >= Config->RequiredArmorPieceCount;
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

void UHeroEquipmentEvolutionComponent::AddCharge(int32 Delta)
{
	URetrieveSaveSubsystem* Save = GetSaveSubsystem();
	if (!Save || !Config)
	{
		return;
	}

	const int32 NewCharge = FMath::Clamp(Save->GetHeroEvolutionCharge() + Delta, 0, Config->ChargeThreshold);
	Save->SetHeroEvolutionCharge(NewCharge);
	BroadcastProgress();

	UE_LOG(LogRetrieveCombat, Log, TEXT("[HeroEvolution] Charge %d/%d"), NewCharge, Config->ChargeThreshold);

	if (NewCharge >= Config->ChargeThreshold && !bEvolving && !bEvolutionScheduled && !IsEvolved())
	{
		// 진화는 흡수/버스트 어빌리티 활성화 콜백 '도중'에 트리거된다. 이 자리에서 장비를 교체하면
		// 재진입(활성 어빌리티 중 ASC 조작)과 전투 상태 게이트 문제가 생기므로 다음 틱으로 미룬다.
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
}

void UHeroEquipmentEvolutionComponent::PerformEvolution()
{
	if (bEvolving || IsEvolved() || !Config || !HasAuthorityToModify())
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

	// 장착은 반드시 컴포넌트(WeaponComponent/ArmorComponent)로 '직접' 한다.
	// InventoryComponent::RequestEquip*는 CanChangeEquipment() 게이트(전투 스탠스 State.Player.Combat 등)에
	// 막혀 전투 중엔 장착이 거부된다. 그러면 잊혀진 아이템만 제거돼 알몸이 되는 버그가 난다.
	// 직접 장착은 게이트를 우회하며, 장착 성공을 확인한 뒤에만 잊혀진 아이템을 회수한다.

	// 1) 무기 스왑
	const FName ForgottenWeaponId = Weapon->GetCurrentWeaponDataRow();
	const FName LegendaryWeaponId = Config->GetEvolvedItemId(ForgottenWeaponId);
	if (!LegendaryWeaponId.IsNone())
	{
		Inv->AddItem(LegendaryWeaponId, Config->WeaponCategoryTag, 1);
		if (Weapon->EquipWeapon(LegendaryWeaponId))
		{
			Inv->RemoveItem(ForgottenWeaponId, Config->WeaponCategoryTag, 1);
		}
	}

	// 2) 방어구 스왑: 반복 중 장착 배열이 바뀌므로 스냅샷을 떠서 순회한다.
	//    잊혀진 방어구 = ItemEvolutionMap의 '키'. (ArmorSetTag 비의존)
	const TArray<FRetrieveEquippedArmorEntry> Entries = Armor->GetEquippedArmorEntries();
	for (const FRetrieveEquippedArmorEntry& Entry : Entries)
	{
		if (Entry.ArmorItemId.IsNone() || !Config->ItemEvolutionMap.Contains(Entry.ArmorItemId))
		{
			continue;
		}
		const FName LegendaryArmorId = Config->GetEvolvedItemId(Entry.ArmorItemId);
		if (LegendaryArmorId.IsNone())
		{
			continue;
		}
		Inv->AddItem(LegendaryArmorId, Config->ArmorCategoryTag, 1);
		// 전설 아이템의 슬롯은 잊혀진 아이템과 동일하므로 Entry.EquipmentSlotTag로 장착된다.
		if (Armor->EquipArmor(Entry.EquipmentSlotTag, LegendaryArmorId))
		{
			Inv->RemoveItem(Entry.ArmorItemId, Config->ArmorCategoryTag, 1);
		}
	}

	// 3) 영속: 진화 완료 플래그 + 충전량 상한 고정.
	if (URetrieveSaveSubsystem* Save = GetSaveSubsystem())
	{
		Save->SetHeroEquipmentEvolved(true);
		Save->SetHeroEvolutionCharge(Config->ChargeThreshold);
	}

	UE_LOG(LogRetrieveCombat, Log, TEXT("[HeroEvolution] 진화 완료 — 전설 영웅 장비로 스왑"));

	bEvolving = false;

	OnEvolutionCompleted.Broadcast();

	// 이제 전설 세트 착용 상태 → 잊혀진 세트 미완성 → 카운트 정지.
	RecomputeSetComplete();
	BroadcastProgress();
}

void UHeroEquipmentEvolutionComponent::BroadcastProgress()
{
	const int32 Charge = GetCharge();
	const int32 Threshold = GetChargeThreshold();
	OnEvolutionProgress.Broadcast(Charge, Threshold, bSetComplete);
}
