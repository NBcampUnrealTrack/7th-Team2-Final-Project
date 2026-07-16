#include "Components/Element/ElementResonanceComponent.h"

#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Engine/DataTable.h"
#include "GameplayEffect.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Logging/RetrieveLogChannels.h"
#include "TimerManager.h"
#include "UI/HUD/RetrieveBuffUIBroadcastComponent.h"

namespace
{
// 컴포넌트에 테이블 미지정 시 사용하는 기본 에셋 경로 (BP 세팅 없이도 동작)
const TCHAR* DefaultResonanceTablePath = TEXT("/Game/Retrieve/Data/Skill/DT_ElementResonance.DT_ElementResonance");
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

	ScheduleRecompute();
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

	if (IsAttuneSpec(RemovedGE.Spec))
	{
		ScheduleRecompute();
	}
}

void UElementResonanceComponent::OnAttuneStackChanged(
	FActiveGameplayEffectHandle Handle, int32 NewStackCount, int32 PreviousStackCount)
{
	ScheduleRecompute();
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
	if (!PinnedASC || !ResonanceTable || !HasAuthorityToModify())
	{
		return;
	}

	// 총 스택(장비 영구 + 흡수) — 공명 조건 판정용
	const int32 FireStacks = CountAttuneStacks(RetrieveGameplayTags::Element_Attune_Fire);
	const int32 WaterStacks = CountAttuneStacks(RetrieveGameplayTags::Element_Attune_Water);
	const int32 WindStacks = CountAttuneStacks(RetrieveGameplayTags::Element_Attune_Wind);

	// 흡수(유한 지속) 스택만 — 소모 대상이자 발동 필수 조건
	const int32 FireAbsorb = CountAbsorbStacks(RetrieveGameplayTags::Element_Attune_Fire);
	const int32 WaterAbsorb = CountAbsorbStacks(RetrieveGameplayTags::Element_Attune_Water);
	const int32 WindAbsorb = CountAbsorbStacks(RetrieveGameplayTags::Element_Attune_Wind);

	// 소모할 흡수 스택이 전혀 없으면(장비 어튠만) 공명을 발동하지 않는다 — "흡수 스택 필수 소모" 규칙.
	if (FireAbsorb <= 0 && WaterAbsorb <= 0 && WindAbsorb <= 0)
	{
		return;
	}

	// 1. 조건 충족 행 수집
	struct FSatisfiedRow
	{
		FName RowName;
		const FElementResonanceRow* Row = nullptr;
	};
	TArray<FSatisfiedRow> Satisfied;

	auto MeetsRequirement = [](int32 Current, int32 Required, bool bExact) -> bool
	{
		if (Required <= 0)
		{
			return true; // 0 = 무관
		}
		return bExact ? (Current == Required) : (Current >= Required);
	};

	for (const TPair<FName, uint8*>& Pair : ResonanceTable->GetRowMap())
	{
		const FElementResonanceRow* Row = reinterpret_cast<const FElementResonanceRow*>(Pair.Value);
		if (!Row || (Row->RequiredFire <= 0 && Row->RequiredWater <= 0 && Row->RequiredWind <= 0))
		{
			continue;
		}

		if (MeetsRequirement(FireStacks, Row->RequiredFire, Row->bExactMatch) &&
			MeetsRequirement(WaterStacks, Row->RequiredWater, Row->bExactMatch) &&
			MeetsRequirement(WindStacks, Row->RequiredWind, Row->bExactMatch))
		{
			Satisfied.Add({ Pair.Key, Row });
		}
	}

	// 2. 배타 그룹: 같은 그룹에서 Priority 최고 하나만 남긴다
	TSet<FName> Desired;
	TMap<FGameplayTag, const FSatisfiedRow*> BestPerGroup;
	for (const FSatisfiedRow& Entry : Satisfied)
	{
		if (Entry.Row->bExclusive && Entry.Row->ExclusiveGroup.IsValid())
		{
			const FSatisfiedRow*& Best = BestPerGroup.FindOrAdd(Entry.Row->ExclusiveGroup);
			if (!Best || Entry.Row->Priority > Best->Row->Priority)
			{
				Best = &Entry;
			}
		}
		else
		{
			Desired.Add(Entry.RowName);
		}
	}
	for (const TPair<FGameplayTag, const FSatisfiedRow*>& Pair : BestPerGroup)
	{
		Desired.Add(Pair.Value->RowName);
	}

	// 3. 아직 활성이 아닌(=새로) 충족된 공명만 발동한다.
	//    기존 활성 공명은 회수하지 않고 각자의 12초 타이머로 자연 만료된다(카운트다운 표시).
	bool bChanged = false;
	bool bConsumeFire = false, bConsumeWater = false, bConsumeWind = false;
	for (const FName& RowName : Desired)
	{
		if (ActiveResonanceHandles.Contains(RowName))
		{
			continue; // 이미 활성 → 재발동 금지(만료 후 재흡수해야 다시 발동)
		}

		const FElementResonanceRow* Row = ResonanceTable->FindRow<FElementResonanceRow>(RowName, TEXT("RecomputeResonance"));
		if (!Row)
		{
			continue;
		}

		// 이 공명이 요구하는 원소 중 최소 하나에 흡수 스택이 있어야 발동(장비 어튠만으로는 발동 불가).
		const bool bAbsorbContributes =
			(Row->RequiredFire  > 0 && FireAbsorb  > 0) ||
			(Row->RequiredWater > 0 && WaterAbsorb > 0) ||
			(Row->RequiredWind  > 0 && WindAbsorb  > 0);
		if (!bAbsorbContributes)
		{
			continue;
		}

		UClass* EffectClass = Row->ResonanceEffect.LoadSynchronous();
		if (!EffectClass)
		{
			UE_LOG(LogRetrieveCombat, Warning, TEXT("[Resonance] Row %s has no valid ResonanceEffect"), *RowName.ToString());
			continue;
		}

		FGameplayEffectContextHandle Context = PinnedASC->MakeEffectContext();
		Context.AddSourceObject(this);
		const FGameplayEffectSpecHandle SpecHandle = PinnedASC->MakeOutgoingSpec(EffectClass, 1.f, Context);
		if (SpecHandle.IsValid())
		{
			const FActiveGameplayEffectHandle Applied = PinnedASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
			ActiveResonanceHandles.Add(RowName, Applied);
			bChanged = true;
			if (Row->RequiredFire  > 0) { bConsumeFire  = true; }
			if (Row->RequiredWater > 0) { bConsumeWater = true; }
			if (Row->RequiredWind  > 0) { bConsumeWind  = true; }
			UE_LOG(LogRetrieveCombat, Log, TEXT("[Resonance] Activated (timed 12s): %s (F=%d W=%d G=%d)"),
				*RowName.ToString(), FireStacks, WaterStacks, WindStacks);
		}
	}

	// 4. 발동된 공명이 요구한 원소의 흡수 스택을 소모한다(재흡수 전까지 재발동 방지).
	if (bConsumeFire)  { ConsumeAbsorbStacks(RetrieveGameplayTags::Element_Attune_Fire); }
	if (bConsumeWater) { ConsumeAbsorbStacks(RetrieveGameplayTags::Element_Attune_Water); }
	if (bConsumeWind)  { ConsumeAbsorbStacks(RetrieveGameplayTags::Element_Attune_Wind); }

	if (bChanged)
	{
		OnResonanceChanged.Broadcast();
	}

	// 공명 칩(12초 카운트다운)은 공명 GE의 UI.Buff.Resonance.* AssetTag로
	// RetrieveBuffUIBroadcastComponent가 OnGEAdded/OnGERemoved에서 자동 표시·제거한다(수동 브로드캐스트 불필요).
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

void UElementResonanceComponent::ConsumeAbsorbStacks(const FGameplayTag& AttuneTag)
{
	UAbilitySystemComponent* PinnedASC = ASC.Get();
	if (!PinnedASC || !AttuneTag.IsValid())
	{
		return;
	}

	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(
		FGameplayTagContainer(AttuneTag));

	for (const FActiveGameplayEffectHandle& Handle : PinnedASC->GetActiveEffects(Query))
	{
		const FActiveGameplayEffect* AGE = PinnedASC->GetActiveGameplayEffect(Handle);
		if (AGE && AGE->Spec.Def &&
			AGE->Spec.Def->DurationPolicy == EGameplayEffectDurationType::HasDuration)
		{
			// 흡수 스택 전체 제거 = 소모 (StacksToRemove=-1: 전부)
			PinnedASC->RemoveActiveGameplayEffect(Handle, -1);
		}
	}
}
