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

	// 공명은 한 번에 하나만 적용된다. 이미 활성 공명이 있으면 새로 발동하지 않으며,
	// 이 동안 흡수해도 어튠 스택은 쌓이지 않는다(GA_Absorb가 HasActiveResonance로 차단).
	if (!ActiveResonanceHandles.IsEmpty())
	{
		return;
	}

	const int32 FireAbsorb = CountAbsorbStacks(RetrieveGameplayTags::Element_Attune_Fire);
	const int32 WaterAbsorb = CountAbsorbStacks(RetrieveGameplayTags::Element_Attune_Water);
	const int32 WindAbsorb = CountAbsorbStacks(RetrieveGameplayTags::Element_Attune_Wind);

	// 흡수 스택이 하나도 없으면 발동할 조합이 없다.
	if (FireAbsorb <= 0 && WaterAbsorb <= 0 && WindAbsorb <= 0)
	{
		return;
	}

	// 장비 등 영구 어튠은 조건 판정에 포함하되(비용 대납) 소모하지 않는다.
	const int32 FireTotal = CountAttuneStacks(RetrieveGameplayTags::Element_Attune_Fire);
	const int32 WaterTotal = CountAttuneStacks(RetrieveGameplayTags::Element_Attune_Water);
	const int32 WindTotal = CountAttuneStacks(RetrieveGameplayTags::Element_Attune_Wind);

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
		if (!Row)
		{
			continue;
		}

		// bExactMatch=true면 요구치와 정확히 일치할 때만(0인 원소는 무관), false면 이상이면 발동.
		bool bMeets = false;
		if (Row->bExactMatch)
		{
			bMeets =
				(Row->RequiredFire == 0 || FireTotal == Row->RequiredFire) &&
				(Row->RequiredWater == 0 || WaterTotal == Row->RequiredWater) &&
				(Row->RequiredWind == 0 || WindTotal == Row->RequiredWind);
		}
		else
		{
			bMeets =
				FireTotal >= Row->RequiredFire &&
				WaterTotal >= Row->RequiredWater &&
				WindTotal >= Row->RequiredWind;
		}

		// 새로 들어온 흡수가 실제로 이 공명에 기여해야 발동한다(장비만으로 상시 발동 방지).
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

	// 원소 종류 많은 것 > 총 요구량 큰 것 > Priority 높은 것 순으로 하나만 고른다.
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

	// 영구 어튠이 대납하고 남은 만큼만 흡수 스택에서 소모한다.
	const int32 FirePersistent = FMath::Max(0, FireTotal - FireAbsorb);
	const int32 WaterPersistent = FMath::Max(0, WaterTotal - WaterAbsorb);
	const int32 WindPersistent = FMath::Max(0, WindTotal - WindAbsorb);

	const int32 ConsumeFire = FMath::Max(0, Selected.Row->RequiredFire - FirePersistent);
	const int32 ConsumeWater = FMath::Max(0, Selected.Row->RequiredWater - WaterPersistent);
	const int32 ConsumeWind = FMath::Max(0, Selected.Row->RequiredWind - WindPersistent);

	// 흡수가 요구량에 못 미치면 발동하지 않는다(방어적).
	if (ConsumeFire > FireAbsorb || ConsumeWater > WaterAbsorb || ConsumeWind > WindAbsorb)
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

	if (ConsumeFire > 0)
	{
		ConsumeAbsorbStacks(RetrieveGameplayTags::Element_Attune_Fire, ConsumeFire);
	}
	if (ConsumeWater > 0)
	{
		ConsumeAbsorbStacks(RetrieveGameplayTags::Element_Attune_Water, ConsumeWater);
	}
	if (ConsumeWind > 0)
	{
		ConsumeAbsorbStacks(RetrieveGameplayTags::Element_Attune_Wind, ConsumeWind);
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

	bApplyingResonanceTransaction = false;
	OnResonanceChanged.Broadcast();

	UE_LOG(LogRetrieveCombat, Log,
		TEXT("[Resonance] Activated %s: consumed absorb F=%d W=%d G=%d"),
		*Selected.RowName.ToString(), ConsumeFire, ConsumeWater, ConsumeWind);
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
