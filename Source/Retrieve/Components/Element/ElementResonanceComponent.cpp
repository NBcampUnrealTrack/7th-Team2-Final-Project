#include "Components/Element/ElementResonanceComponent.h"

#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Engine/DataTable.h"
#include "GameplayEffect.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Logging/RetrieveLogChannels.h"
#include "TimerManager.h"
#include "UI/HUD/RetrieveBuffUIBroadcastComponent.h"
#include "UObject/SoftObjectPath.h"

namespace
{
// 컴포넌트에 테이블 미지정 시 사용하는 기본 에셋 경로 (BP 세팅 없이도 동작)
const TCHAR* DefaultResonanceTablePath = TEXT("/Game/Retrieve/Data/Skill/DT_ElementResonance.DT_ElementResonance");


FGameplayTag AttuneToAbsorbBuffTag(const FGameplayTag& AttuneTag)
{
	if (AttuneTag.MatchesTagExact(RetrieveGameplayTags::Element_Attune_Fire))
	{
		return RetrieveGameplayTags::UI_Buff_Absorb_Fire;
	}
	if (AttuneTag.MatchesTagExact(RetrieveGameplayTags::Element_Attune_Water))
	{
		return RetrieveGameplayTags::UI_Buff_Absorb_Water;
	}
	if (AttuneTag.MatchesTagExact(RetrieveGameplayTags::Element_Attune_Wind))
	{
		return RetrieveGameplayTags::UI_Buff_Absorb_Wind;
	}
	return FGameplayTag();
}

UClass* LoadAbsorbEffectClass(const FGameplayTag& AttuneTag)
{
	const TCHAR* EffectPath = nullptr;
	if (AttuneTag.MatchesTagExact(RetrieveGameplayTags::Element_Attune_Fire))
	{
		EffectPath = TEXT("/Game/Retrieve/AbilitySystem/Player/GE_Absorb_Fire.GE_Absorb_Fire_C");
	}
	else if (AttuneTag.MatchesTagExact(RetrieveGameplayTags::Element_Attune_Water))
	{
		EffectPath = TEXT("/Game/Retrieve/AbilitySystem/Player/GE_Absorb_Water.GE_Absorb_Water_C");
	}
	else if (AttuneTag.MatchesTagExact(RetrieveGameplayTags::Element_Attune_Wind))
	{
		EffectPath = TEXT("/Game/Retrieve/AbilitySystem/Player/GE_Absorb_Wind.GE_Absorb_Wind_C");
	}
	return EffectPath ? FSoftClassPath(EffectPath).TryLoadClass<UGameplayEffect>() : nullptr;
}

UClass* LoadAttuneStackEffectClass(const FGameplayTag& AttuneTag)
{
	const TCHAR* EffectPath = nullptr;
	if (AttuneTag.MatchesTagExact(RetrieveGameplayTags::Element_Attune_Fire))
	{
		EffectPath = TEXT("/Game/Retrieve/AbilitySystem/Player/Resonance/GE_ElementStack_Fire.GE_ElementStack_Fire_C");
	}
	else if (AttuneTag.MatchesTagExact(RetrieveGameplayTags::Element_Attune_Water))
	{
		EffectPath = TEXT("/Game/Retrieve/AbilitySystem/Player/Resonance/GE_ElementStack_Water.GE_ElementStack_Water_C");
	}
	else if (AttuneTag.MatchesTagExact(RetrieveGameplayTags::Element_Attune_Wind))
	{
		EffectPath = TEXT("/Game/Retrieve/AbilitySystem/Player/Resonance/GE_ElementStack_Wind.GE_ElementStack_Wind_C");
	}
	return EffectPath ? FSoftClassPath(EffectPath).TryLoadClass<UGameplayEffect>() : nullptr;
}
}

UElementResonanceComponent::UElementResonanceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UElementResonanceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UninitializeFromAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

bool UElementResonanceComponent::HasAuthorityToModify() const
{
	const AActor* Owner = GetOwner();
	return !Owner || Owner->HasAuthority();
}

void UElementResonanceComponent::ResolveResonanceTable()
{
	if (!ResonanceTable)
	{
		ResonanceTable = Cast<UDataTable>(
			FSoftObjectPath(DefaultResonanceTablePath).TryLoad());
	}
}

void UElementResonanceComponent::InitializeWithAbilitySystem(URetrieveAbilitySystemComponent* InASC)
{
	if (!InASC || !HasAuthorityToModify())
	{
		return;
	}

	if (ASC.Get() == InASC)
	{
		return;
	}

	UninitializeFromAbilitySystem();
	ASC = InASC;
	ResolveResonanceTable();

	GEAddedHandle = InASC->OnActiveGameplayEffectAddedDelegateToSelf.AddUObject(
		this, &UElementResonanceComponent::OnGameplayEffectAdded);
	GERemovedHandle = InASC->OnAnyGameplayEffectRemovedDelegate().AddUObject(
		this, &UElementResonanceComponent::OnGameplayEffectRemoved);

	// 초기화 시점에 이미 어튠 GE가 붙어있을 수 있으므로(장비 선장착) 1회 계산
	ScheduleRecompute();
}

void UElementResonanceComponent::UninitializeFromAbilitySystem()
{
	if (UAbilitySystemComponent* PinnedASC = ASC.Get())
	{
		if (GEAddedHandle.IsValid())
		{
			PinnedASC->OnActiveGameplayEffectAddedDelegateToSelf.Remove(GEAddedHandle);
		}
		if (GERemovedHandle.IsValid())
		{
			PinnedASC->OnAnyGameplayEffectRemovedDelegate().Remove(GERemovedHandle);
		}

		// 부여했던 공명 GE 전부 회수
		for (const TPair<FName, FActiveGameplayEffectHandle>& Pair : ActiveResonanceHandles)
		{
			if (Pair.Value.IsValid())
			{
				PinnedASC->RemoveActiveGameplayEffect(Pair.Value);
			}
		}
	}

	GEAddedHandle.Reset();
	GERemovedHandle.Reset();
	WatchedStackHandles.Reset();
	ActiveResonanceHandles.Reset();
	ASC = nullptr;
}

void UElementResonanceComponent::RestoreAbsorbStacks(
	const FGameplayTag& AttuneTag, int32 StacksToRestore)
{
	UAbilitySystemComponent* PinnedASC = ASC.Get();
	if (!PinnedASC || !AttuneTag.IsValid() || StacksToRestore <= 0)
	{
		return;
	}

	UClass* StackEffectClass = LoadAttuneStackEffectClass(AttuneTag);
	UClass* AbsorbEffectClass = LoadAbsorbEffectClass(AttuneTag);
	if (!StackEffectClass || !AbsorbEffectClass)
	{
		return;
	}

	FActiveGameplayEffectHandle LastAbsorbHandle;
	float AbsorbDuration = 0.f;
	for (int32 Index = 0; Index < StacksToRestore; ++Index)
	{
		FGameplayEffectContextHandle Context = PinnedASC->MakeEffectContext();
		Context.AddSourceObject(this);

		const FGameplayEffectSpecHandle StackSpec =
			PinnedASC->MakeOutgoingSpec(StackEffectClass, 1.f, Context);
		if (StackSpec.IsValid())
		{
			PinnedASC->ApplyGameplayEffectSpecToSelf(*StackSpec.Data);
		}

		const FGameplayEffectSpecHandle AbsorbSpec =
			PinnedASC->MakeOutgoingSpec(AbsorbEffectClass, 1.f, Context);
		if (AbsorbSpec.IsValid())
		{
			AbsorbDuration = AbsorbSpec.Data->GetDuration();
			LastAbsorbHandle = PinnedASC->ApplyGameplayEffectSpecToSelf(*AbsorbSpec.Data);
		}
	}

	if (AActor* Owner = GetOwner())
	{
		if (URetrieveBuffUIBroadcastComponent* BuffUI =
			Owner->FindComponentByClass<URetrieveBuffUIBroadcastComponent>())
		{
			const FGameplayTag BuffTag = AttuneToAbsorbBuffTag(AttuneTag);
			if (BuffTag.IsValid())
			{
				const int32 StackCount = LastAbsorbHandle.IsValid()
					? PinnedASC->GetCurrentStackCount(LastAbsorbHandle)
					: StacksToRestore;
				BuffUI->BroadcastBuffManual(
					BuffTag, AbsorbDuration > 0.f ? AbsorbDuration : 0.f,
					AbsorbEffectClass, StackCount);
			}
		}
	}
}

bool UElementResonanceComponent::IsAttuneSpec(const FGameplayEffectSpec& Spec)
{
	FGameplayTagContainer GrantedTags;
	Spec.GetAllGrantedTags(GrantedTags);
	return GrantedTags.HasTag(RetrieveGameplayTags::Element_Attune);
}

void UElementResonanceComponent::OnGameplayEffectAdded(
	UAbilitySystemComponent* InASC, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle)
{
	if (!IsAttuneSpec(Spec))
	{
		return;
	}

	// 스택형 어튠 GE는 스택 수 변화도 재계산 트리거로 삼는다
	if (InASC && !WatchedStackHandles.Contains(Handle))
	{
		if (FOnActiveGameplayEffectStackChange* StackDelegate = InASC->OnGameplayEffectStackChangeDelegate(Handle))
		{
			StackDelegate->AddUObject(this, &UElementResonanceComponent::OnAttuneStackChanged);
			WatchedStackHandles.Add(Handle);
		}
	}

	if (!bApplyingResonanceTransaction)
	{
		ScheduleRecompute();
	}
}

void UElementResonanceComponent::OnGameplayEffectRemoved(const FActiveGameplayEffect& RemovedGE)
{
	WatchedStackHandles.Remove(RemovedGE.Handle);

	// 12초 만료(또는 회수)된 공명 핸들을 목록에서 지워, 재흡수 시 같은 공명을 다시 발동할 수 있게 한다.
	bool bRemovedResonance = false;
	for (auto It = ActiveResonanceHandles.CreateIterator(); It; ++It)
	{
		if (It.Value() == RemovedGE.Handle)
		{
			It.RemoveCurrent();
			bRemovedResonance = true;
		}
	}
	if (bRemovedResonance)
	{
		OnResonanceChanged.Broadcast();
	}

	if (IsAttuneSpec(RemovedGE.Spec) && !bApplyingResonanceTransaction)
	{
		ScheduleRecompute();
	}
}

void UElementResonanceComponent::OnAttuneStackChanged(
	FActiveGameplayEffectHandle Handle, int32 NewStackCount, int32 PreviousStackCount)
{
	if (!bApplyingResonanceTransaction)
	{
		ScheduleRecompute();
	}
}

void UElementResonanceComponent::ScheduleRecompute()
{
	// GE 추가/제거 델리게이트 안에서 다른 GE를 적용/제거하는 재진입을 피해 다음 틱에 1회만 계산
	if (bRecomputePending)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	bRecomputePending = true;
	World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			bRecomputePending = false;
			RecomputeResonance();
		}));
}

int32 UElementResonanceComponent::CountAttuneStacks(const FGameplayTag& AttuneTag) const
{
	const UAbilitySystemComponent* PinnedASC = ASC.Get();
	if (!PinnedASC)
	{
		return 0;
	}

	// 같은 어튠 태그를 부여하는 활성 GE들의 스택 수 합 = 영속(장비) + 전투 축적(흡수/아이템)
	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(
		FGameplayTagContainer(AttuneTag));

	int32 Total = 0;
	for (const FActiveGameplayEffectHandle& Handle :
		const_cast<UAbilitySystemComponent*>(PinnedASC)->GetActiveEffects(Query))
	{
		Total += FMath::Max(1, PinnedASC->GetCurrentStackCount(Handle));
	}
	return Total;
}

int32 UElementResonanceComponent::GetElementStackCount(FGameplayTag AttuneTag) const
{
	return AttuneTag.IsValid() ? CountAttuneStacks(AttuneTag) : 0;
}

TArray<FName> UElementResonanceComponent::GetActiveResonanceIds() const
{
	TArray<FName> Ids;
	ActiveResonanceHandles.GenerateKeyArray(Ids);
	return Ids;
}

void UElementResonanceComponent::RecomputeResonance()
{
	UAbilitySystemComponent* PinnedASC = ASC.Get();
	if (!PinnedASC || !ResonanceTable || !HasAuthorityToModify() || bApplyingResonanceTransaction)
	{
		return;
	}

	const int32 FireTotal = CountAttuneStacks(RetrieveGameplayTags::Element_Attune_Fire);
	const int32 WaterTotal = CountAttuneStacks(RetrieveGameplayTags::Element_Attune_Water);
	const int32 WindTotal = CountAttuneStacks(RetrieveGameplayTags::Element_Attune_Wind);
	const int32 FireAbsorb = CountAbsorbStacks(RetrieveGameplayTags::Element_Attune_Fire);
	const int32 WaterAbsorb = CountAbsorbStacks(RetrieveGameplayTags::Element_Attune_Water);
	const int32 WindAbsorb = CountAbsorbStacks(RetrieveGameplayTags::Element_Attune_Wind);

	// 외부 흡수/아이템 스택이 새로 들어온 경우에만 조합을 시작한다.
	if (FireAbsorb <= 0 && WaterAbsorb <= 0 && WindAbsorb <= 0)
	{
		return;
	}

	const int32 FirePersistent = FMath::Max(0, FireTotal - FireAbsorb);
	const int32 WaterPersistent = FMath::Max(0, WaterTotal - WaterAbsorb);
	const int32 WindPersistent = FMath::Max(0, WindTotal - WindAbsorb);

	struct FActiveMaterial
	{
		FName RowName;
		FActiveGameplayEffectHandle Handle;
		const FElementResonanceRow* Row = nullptr;
	};
	TArray<FActiveMaterial> ActiveMaterials;

	int32 AvailableFire = FireAbsorb + FirePersistent;
	int32 AvailableWater = WaterAbsorb + WaterPersistent;
	int32 AvailableWind = WindAbsorb + WindPersistent;
	for (const TPair<FName, FActiveGameplayEffectHandle>& Pair : ActiveResonanceHandles)
	{
		const FElementResonanceRow* ActiveRow =
			ResonanceTable->FindRow<FElementResonanceRow>(Pair.Key, TEXT("BuildResonanceMaterials"));
		if (!ActiveRow)
		{
			continue;
		}

		AvailableFire += ActiveRow->RequiredFire;
		AvailableWater += ActiveRow->RequiredWater;
		AvailableWind += ActiveRow->RequiredWind;
		ActiveMaterials.Add({ Pair.Key, Pair.Value, ActiveRow });
	}

	struct FCandidate
	{
		FName RowName;
		const FElementResonanceRow* Row = nullptr;
		int32 ElementKinds = 0;
		int32 TotalCost = 0;
	};
	TArray<FCandidate> Candidates;

	for (const TPair<FName, uint8*>& Pair : ResonanceTable->GetRowMap())
	{
		const FElementResonanceRow* Row = reinterpret_cast<const FElementResonanceRow*>(Pair.Value);
		if (!Row || ActiveResonanceHandles.Contains(Pair.Key))
		{
			continue;
		}

		const bool bMeets =
			AvailableFire >= Row->RequiredFire &&
			AvailableWater >= Row->RequiredWater &&
			AvailableWind >= Row->RequiredWind;
		const bool bNewAbsorbContributes =
			(Row->RequiredFire > 0 && FireAbsorb > 0) ||
			(Row->RequiredWater > 0 && WaterAbsorb > 0) ||
			(Row->RequiredWind > 0 && WindAbsorb > 0);
		if (!bMeets || !bNewAbsorbContributes)
		{
			continue;
		}

		const int32 Kinds =
			(Row->RequiredFire > 0 ? 1 : 0) +
			(Row->RequiredWater > 0 ? 1 : 0) +
			(Row->RequiredWind > 0 ? 1 : 0);
		Candidates.Add({
			Pair.Key,
			Row,
			Kinds,
			Row->RequiredFire + Row->RequiredWater + Row->RequiredWind
		});
	}

	if (Candidates.IsEmpty())
	{
		return;
	}

	Candidates.Sort([](const FCandidate& A, const FCandidate& B)
	{
		if (A.ElementKinds != B.ElementKinds)
		{
			return A.ElementKinds > B.ElementKinds;
		}
		if (A.TotalCost != B.TotalCost)
		{
			return A.TotalCost > B.TotalCost;
		}
		return A.Row->Priority > B.Row->Priority;
	});
	const FCandidate& Selected = Candidates[0];

	int32 NeedFire = Selected.Row->RequiredFire;
	int32 NeedWater = Selected.Row->RequiredWater;
	int32 NeedWind = Selected.Row->RequiredWind;
	int32 RefundFire = 0;
	int32 RefundWater = 0;
	int32 RefundWind = 0;
	TArray<FName> ConsumedResonanceRows;
	TArray<FActiveGameplayEffectHandle> ConsumedResonanceHandles;

	// 기존 공명을 먼저 재료로 사용한다. 일부만 사용하면 나머지 원소는 흡수 스택으로 환급한다.
	ActiveMaterials.Sort([](const FActiveMaterial& A, const FActiveMaterial& B)
	{
		const int32 ACost = A.Row->RequiredFire + A.Row->RequiredWater + A.Row->RequiredWind;
		const int32 BCost = B.Row->RequiredFire + B.Row->RequiredWater + B.Row->RequiredWind;
		return ACost > BCost;
	});
	for (const FActiveMaterial& Material : ActiveMaterials)
	{
		const int32 UseFire = FMath::Min(NeedFire, Material.Row->RequiredFire);
		const int32 UseWater = FMath::Min(NeedWater, Material.Row->RequiredWater);
		const int32 UseWind = FMath::Min(NeedWind, Material.Row->RequiredWind);
		if (UseFire + UseWater + UseWind <= 0)
		{
			continue;
		}

		NeedFire -= UseFire;
		NeedWater -= UseWater;
		NeedWind -= UseWind;
		RefundFire += Material.Row->RequiredFire - UseFire;
		RefundWater += Material.Row->RequiredWater - UseWater;
		RefundWind += Material.Row->RequiredWind - UseWind;
		ConsumedResonanceRows.Add(Material.RowName);
		ConsumedResonanceHandles.Add(Material.Handle);

		if (NeedFire <= 0 && NeedWater <= 0 && NeedWind <= 0)
		{
			break;
		}
	}

	// 장비 세트의 영구 어튠은 비용을 대신하지만 제거되지는 않는다.
	const int32 UsePersistentFire = FMath::Min(NeedFire, FirePersistent);
	const int32 UsePersistentWater = FMath::Min(NeedWater, WaterPersistent);
	const int32 UsePersistentWind = FMath::Min(NeedWind, WindPersistent);
	NeedFire -= UsePersistentFire;
	NeedWater -= UsePersistentWater;
	NeedWind -= UsePersistentWind;

	if (NeedFire > FireAbsorb || NeedWater > WaterAbsorb || NeedWind > WindAbsorb)
	{
		return;
	}

	UClass* EffectClass = Selected.Row->ResonanceEffect.LoadSynchronous();
	if (!EffectClass)
	{
		UE_LOG(LogRetrieveCombat, Warning,
			TEXT("[Resonance] Row %s has no valid ResonanceEffect"), *Selected.RowName.ToString());
		return;
	}

	bApplyingResonanceTransaction = true;

	for (const FName& RowName : ConsumedResonanceRows)
	{
		ActiveResonanceHandles.Remove(RowName);
	}
	for (const FActiveGameplayEffectHandle& Handle : ConsumedResonanceHandles)
	{
		if (Handle.IsValid())
		{
			PinnedASC->RemoveActiveGameplayEffect(Handle);
		}
	}

	if (NeedFire > 0)
	{
		ConsumeAbsorbStacks(RetrieveGameplayTags::Element_Attune_Fire, NeedFire);
	}
	if (NeedWater > 0)
	{
		ConsumeAbsorbStacks(RetrieveGameplayTags::Element_Attune_Water, NeedWater);
	}
	if (NeedWind > 0)
	{
		ConsumeAbsorbStacks(RetrieveGameplayTags::Element_Attune_Wind, NeedWind);
	}

	FGameplayEffectContextHandle Context = PinnedASC->MakeEffectContext();
	Context.AddSourceObject(this);
	const FGameplayEffectSpecHandle SpecHandle =
		PinnedASC->MakeOutgoingSpec(EffectClass, 1.f, Context);
	if (SpecHandle.IsValid())
	{
		const FActiveGameplayEffectHandle Applied =
			PinnedASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
		ActiveResonanceHandles.Add(Selected.RowName, Applied);
	}

	if (RefundFire > 0)
	{
		RestoreAbsorbStacks(RetrieveGameplayTags::Element_Attune_Fire, RefundFire);
	}
	if (RefundWater > 0)
	{
		RestoreAbsorbStacks(RetrieveGameplayTags::Element_Attune_Water, RefundWater);
	}
	if (RefundWind > 0)
	{
		RestoreAbsorbStacks(RetrieveGameplayTags::Element_Attune_Wind, RefundWind);
	}

	bApplyingResonanceTransaction = false;
	OnResonanceChanged.Broadcast();

	UE_LOG(LogRetrieveCombat, Log,
		TEXT("[Resonance] Converted to %s: raw consume F=%d W=%d G=%d, refund F=%d W=%d G=%d"),
		*Selected.RowName.ToString(), NeedFire, NeedWater, NeedWind,
		RefundFire, RefundWater, RefundWind);
}

int32 UElementResonanceComponent::CountAbsorbStacks(const FGameplayTag& AttuneTag) const
{
	UAbilitySystemComponent* PinnedASC = ASC.Get();
	if (!PinnedASC || !AttuneTag.IsValid())
	{
		return 0;
	}

	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(
		FGameplayTagContainer(AttuneTag));

	int32 Total = 0;
	for (const FActiveGameplayEffectHandle& Handle :
		const_cast<UAbilitySystemComponent*>(PinnedASC)->GetActiveEffects(Query))
	{
		const FActiveGameplayEffect* AGE = PinnedASC->GetActiveGameplayEffect(Handle);
		if (AGE && AGE->Spec.Def &&
			AGE->Spec.Def->DurationPolicy == EGameplayEffectDurationType::HasDuration)
		{
			Total += FMath::Max(1, PinnedASC->GetCurrentStackCount(Handle));
		}
	}
	return Total;
}

void UElementResonanceComponent::ConsumeAbsorbStacks(
	const FGameplayTag& AttuneTag, int32 StacksToConsume)
{
	UAbilitySystemComponent* PinnedASC = ASC.Get();
	if (!PinnedASC || !AttuneTag.IsValid() || StacksToConsume <= 0)
	{
		return;
	}

	const FGameplayEffectQuery AttuneQuery =
		FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(AttuneTag));

	int32 RemainingAttune = StacksToConsume;
	for (const FActiveGameplayEffectHandle& Handle : PinnedASC->GetActiveEffects(AttuneQuery))
	{
		const FActiveGameplayEffect* AGE = PinnedASC->GetActiveGameplayEffect(Handle);
		if (AGE && AGE->Spec.Def &&
			AGE->Spec.Def->DurationPolicy == EGameplayEffectDurationType::HasDuration)
		{
			const int32 RemoveCount = FMath::Min(
				RemainingAttune, FMath::Max(1, PinnedASC->GetCurrentStackCount(Handle)));
			PinnedASC->RemoveActiveGameplayEffect(Handle, RemoveCount);
			RemainingAttune -= RemoveCount;
			if (RemainingAttune <= 0)
			{
				break;
			}
		}
	}

	// GE AssetTag 수정 없이 실제 흡수 능력치 버프 클래스를 직접 찾아 같은 수만큼 소비한다.
	UClass* AbsorbEffectClass = LoadAbsorbEffectClass(AttuneTag);
	if (!AbsorbEffectClass)
	{
		return;
	}

	int32 RemainingBuff = StacksToConsume;
	const FGameplayEffectQuery AllEffectsQuery;
	for (const FActiveGameplayEffectHandle& Handle : PinnedASC->GetActiveEffects(AllEffectsQuery))
	{
		const FActiveGameplayEffect* AGE = PinnedASC->GetActiveGameplayEffect(Handle);
		if (!AGE || !AGE->Spec.Def || AGE->Spec.Def->GetClass() != AbsorbEffectClass)
		{
			continue;
		}

		const int32 RemoveCount = FMath::Min(
			RemainingBuff, FMath::Max(1, PinnedASC->GetCurrentStackCount(Handle)));
		PinnedASC->RemoveActiveGameplayEffect(Handle, RemoveCount);
		RemainingBuff -= RemoveCount;
		if (RemainingBuff <= 0)
		{
			break;
		}
	}

	// 흡수 UI는 GA_Absorb에서 수동 발행하므로 조기 소비 시 수동으로 제거/갱신한다.
	if (AActor* Owner = GetOwner())
	{
		if (URetrieveBuffUIBroadcastComponent* BuffUI =
			Owner->FindComponentByClass<URetrieveBuffUIBroadcastComponent>())
		{
			const FGameplayTag BuffTag = AttuneToAbsorbBuffTag(AttuneTag);
			if (BuffTag.IsValid())
			{
				BuffUI->BroadcastBuffRemove(BuffTag);
			}
		}
	}
}
