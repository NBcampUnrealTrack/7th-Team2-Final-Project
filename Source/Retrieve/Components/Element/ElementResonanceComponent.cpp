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

	const int32 FireStacks = CountAttuneStacks(RetrieveGameplayTags::Element_Attune_Fire);
	const int32 WaterStacks = CountAttuneStacks(RetrieveGameplayTags::Element_Attune_Water);
	const int32 WindStacks = CountAttuneStacks(RetrieveGameplayTags::Element_Attune_Wind);

	// 스택 자체를 항상 버프 바에 노출 — 정수 1개/흡수 1회로는 공명이 안 떠도 진행도가 보여야 한다.


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

	// 3. diff 적용 — 해제된 공명 회수
	bool bChanged = false;
	for (auto It = ActiveResonanceHandles.CreateIterator(); It; ++It)
	{
		if (!Desired.Contains(It.Key()))
		{
			if (It.Value().IsValid())
			{
				PinnedASC->RemoveActiveGameplayEffect(It.Value());
			}
			UE_LOG(LogRetrieveCombat, Log, TEXT("[Resonance] Deactivated: %s (F=%d W=%d G=%d)"),
				*It.Key().ToString(), FireStacks, WaterStacks, WindStacks);
			It.RemoveCurrent();
			bChanged = true;
		}
	}

	// 4. diff 적용 — 새로 충족된 공명 부여
	for (const FName& RowName : Desired)
	{
		if (ActiveResonanceHandles.Contains(RowName))
		{
			continue;
		}

		const FElementResonanceRow* Row = ResonanceTable->FindRow<FElementResonanceRow>(RowName, TEXT("RecomputeResonance"));
		if (!Row)
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
			UE_LOG(LogRetrieveCombat, Log, TEXT("[Resonance] Activated: %s (F=%d W=%d G=%d)"),
				*RowName.ToString(), FireStacks, WaterStacks, WindStacks);
			bChanged = true;
		}
	}

	if (bChanged)
	{
		OnResonanceChanged.Broadcast();
	}

	BroadcastCompressedBuffChips(FireStacks, WaterStacks, WindStacks, Desired);
}

void UElementResonanceComponent::BroadcastCompressedBuffChips(
	int32 FireStacks,
	int32 WaterStacks,
	int32 WindStacks,
	const TSet<FName>& ActiveRows) const
{
	AActor* Owner = GetOwner();
	URetrieveBuffUIBroadcastComponent* BuffUI =
		Owner ? Owner->FindComponentByClass<URetrieveBuffUIBroadcastComponent>() : nullptr;
	if (!BuffUI)
	{
		return;
	}

	const FGameplayTag ResonanceTags[] =
	{
		RetrieveGameplayTags::UI_Buff_Resonance_Fire,
		RetrieveGameplayTags::UI_Buff_Resonance_Fire2,
		RetrieveGameplayTags::UI_Buff_Resonance_Water,
		RetrieveGameplayTags::UI_Buff_Resonance_Water2,
		RetrieveGameplayTags::UI_Buff_Resonance_Wind,
		RetrieveGameplayTags::UI_Buff_Resonance_Wind2,
		RetrieveGameplayTags::UI_Buff_Resonance_Steam,
		RetrieveGameplayTags::UI_Buff_Resonance_Storm,
		RetrieveGameplayTags::UI_Buff_Resonance_Mist,
		RetrieveGameplayTags::UI_Buff_Resonance_Trinity,
	};
	for (const FGameplayTag& Tag : ResonanceTags)
	{
		BuffUI->BroadcastBuffRemove(Tag);
	}

	const bool bTrinity2 = ActiveRows.Contains(TEXT("Resonance_Trinity2"));
	const bool bTrinity1 = ActiveRows.Contains(TEXT("Resonance_Trinity1"));
	const bool bHasTrinity = bTrinity2 || bTrinity1;

	bool bFireCovered = bHasTrinity;
	bool bWaterCovered = bHasTrinity;
	bool bWindCovered = bHasTrinity;

	if (bHasTrinity)
	{
		BuffUI->BroadcastBuffManual(
			RetrieveGameplayTags::UI_Buff_Resonance_Trinity,
			0.f,
			nullptr,
			bTrinity2 ? 2 : 1);
	}
	else
	{
		auto ShowSingle = [BuffUI, &ActiveRows](
			const TCHAR* Level2Row,
			const TCHAR* Level1Row,
			FGameplayTag Level2Tag,
			FGameplayTag Level1Tag,
			int32 StackCount) -> bool
		{
			if (ActiveRows.Contains(FName(Level2Row)))
			{
				BuffUI->BroadcastBuffManual(Level2Tag, 0.f, nullptr, FMath::Min(StackCount, 4));
				return true;
			}
			if (ActiveRows.Contains(FName(Level1Row)))
			{
				BuffUI->BroadcastBuffManual(Level1Tag, 0.f, nullptr, FMath::Min(StackCount, 4));
				return true;
			}
			return false;
		};

		bFireCovered = ShowSingle(TEXT("Resonance_Fire2"), TEXT("Resonance_Fire1"),
			RetrieveGameplayTags::UI_Buff_Resonance_Fire2,
			RetrieveGameplayTags::UI_Buff_Resonance_Fire,
			FireStacks);
		bWaterCovered = ShowSingle(TEXT("Resonance_Water2"), TEXT("Resonance_Water1"),
			RetrieveGameplayTags::UI_Buff_Resonance_Water2,
			RetrieveGameplayTags::UI_Buff_Resonance_Water,
			WaterStacks);
		bWindCovered = ShowSingle(TEXT("Resonance_Wind2"), TEXT("Resonance_Wind1"),
			RetrieveGameplayTags::UI_Buff_Resonance_Wind2,
			RetrieveGameplayTags::UI_Buff_Resonance_Wind,
			WindStacks);

		if (ActiveRows.Contains(TEXT("Resonance_Steam")))
		{
			BuffUI->BroadcastBuffManual(RetrieveGameplayTags::UI_Buff_Resonance_Steam);
			bFireCovered = true;
			bWaterCovered = true;
		}
		if (ActiveRows.Contains(TEXT("Resonance_Storm")))
		{
			BuffUI->BroadcastBuffManual(RetrieveGameplayTags::UI_Buff_Resonance_Storm);
			bFireCovered = true;
			bWindCovered = true;
		}
		if (ActiveRows.Contains(TEXT("Resonance_Mist")))
		{
			BuffUI->BroadcastBuffManual(RetrieveGameplayTags::UI_Buff_Resonance_Mist);
			bWaterCovered = true;
			bWindCovered = true;
		}
	}

	const struct
	{
		FGameplayTag UITag;
		int32 Stacks;
		bool bCovered;
	} AttuneChips[] =
	{
		{ RetrieveGameplayTags::UI_Buff_Attune_Fire, FireStacks, bFireCovered },
		{ RetrieveGameplayTags::UI_Buff_Attune_Water, WaterStacks, bWaterCovered },
		{ RetrieveGameplayTags::UI_Buff_Attune_Wind, WindStacks, bWindCovered },
	};

	for (const auto& Chip : AttuneChips)
	{
		// 공명 조건을 충족하기 전의 어튠 진행 스택은 HUD 버프로 노출하지 않는다.
		BuffUI->BroadcastBuffRemove(Chip.UITag);
	}
}