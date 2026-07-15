#include "UI/Skill/RetrieveSkillOverviewWidget.h"

#include "Components/Element/ElementResonanceComponent.h"
#include "Components/Player/ArmorComponent.h"
#include "Components/Player/WeaponComponent.h"
#include "Components/TextBlock.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Engine/DataTable.h"
#include "GameFramework/Pawn.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Player/RetrievePlayerState.h"
#include "TimerManager.h"
#include "UI/RetrieveElementUILibrary.h"

void URetrieveSkillOverviewWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UWeaponComponent* Weapon = GetWeaponComponent())
	{
		Weapon->OnWeaponEquipped.AddUniqueDynamic(this, &ThisClass::HandleWeaponChanged);
		Weapon->OnWeaponUnequipped.AddUniqueDynamic(this, &ThisClass::HandleWeaponChanged);
	}
	if (UArmorComponent* Armor = GetArmorComponent())
	{
		Armor->OnArmorEquipped.AddUniqueDynamic(this, &ThisClass::HandleArmorChanged);
		Armor->OnArmorUnequipped.AddUniqueDynamic(this, &ThisClass::HandleArmorChanged);
	}
	if (UElementResonanceComponent* Resonance = GetResonanceComponent())
	{
		Resonance->OnResonanceChanged.AddUniqueDynamic(this, &ThisClass::HandleResonanceChanged);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RefreshTimerHandle,
			FTimerDelegate::CreateUObject(this, &ThisClass::HandleTimerRefresh),
			FMath::Max(0.1f, RefreshInterval),
			true);
	}
	RefreshAll();
}

void URetrieveSkillOverviewWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}
	if (UWeaponComponent* Weapon = GetWeaponComponent())
	{
		Weapon->OnWeaponEquipped.RemoveDynamic(this, &ThisClass::HandleWeaponChanged);
		Weapon->OnWeaponUnequipped.RemoveDynamic(this, &ThisClass::HandleWeaponChanged);
	}
	if (UArmorComponent* Armor = GetArmorComponent())
	{
		Armor->OnArmorEquipped.RemoveDynamic(this, &ThisClass::HandleArmorChanged);
		Armor->OnArmorUnequipped.RemoveDynamic(this, &ThisClass::HandleArmorChanged);
	}
	if (UElementResonanceComponent* Resonance = GetResonanceComponent())
	{
		Resonance->OnResonanceChanged.RemoveDynamic(this, &ThisClass::HandleResonanceChanged);
	}
	Super::NativeDestruct();
}

void URetrieveSkillOverviewWidget::HandleWeaponChanged(FName) { RefreshAll(); }
void URetrieveSkillOverviewWidget::HandleArmorChanged(FGameplayTag, FName) { RefreshAll(); }
void URetrieveSkillOverviewWidget::HandleResonanceChanged() { RefreshAll(); }
void URetrieveSkillOverviewWidget::HandleTimerRefresh() { RefreshAll(); }

void URetrieveSkillOverviewWidget::RefreshAll()
{
	if (Text_WeaponSection) Text_WeaponSection->SetText(BuildWeaponSection());
	if (Text_SkillSection) Text_SkillSection->SetText(BuildSkillSection(GetCurrentElementTag()));

	UTextBlock* FireText = Text_SkillFire
		? Text_SkillFire.Get()
		: Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_SkillFireSection")));
	UTextBlock* WaterText = Text_SkillWater
		? Text_SkillWater.Get()
		: Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_SkillWaterSection")));
	UTextBlock* WindText = Text_SkillWind
		? Text_SkillWind.Get()
		: Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_SkillWindSection")));

	if (FireText) FireText->SetText(BuildSkillSection(RetrieveGameplayTags::Element_Fire));
	if (WaterText) WaterText->SetText(BuildSkillSection(RetrieveGameplayTags::Element_Water));
	if (WindText) WindText->SetText(BuildSkillSection(RetrieveGameplayTags::Element_Wind));
	if (Text_SetSection) Text_SetSection->SetText(BuildSetSection());
	if (Text_ResonanceSection) Text_ResonanceSection->SetText(BuildResonanceSection());
	if (Text_AdvantageSection) Text_AdvantageSection->SetText(BuildAdvantageSection());
}

FText URetrieveSkillOverviewWidget::BuildWeaponSection() const
{
	const UWeaponComponent* Weapon = GetWeaponComponent();
	if (!Weapon)
	{
		return FText::FromString(TEXT("무기 미장착"));
	}

	const FRetrieveWeaponDataRow& Data = Weapon->GetWeaponDataRef();
	TArray<FString> Lines;
	Lines.Add(Data.DisplayName.ToString());
	Lines.Add(FString::Printf(TEXT("공격력 %.0f · 원소 충전 ×%.2f"), Data.AttackPower, Data.ElementChargeMultiplier));
	if (!Data.ShortDescription.IsEmpty())
	{
		Lines.Add(FString::Printf(TEXT("장비 효과: %s"), *Data.ShortDescription.ToString()));
	}
	return FText::FromString(FString::Join(Lines, TEXT("\n")));
}

FText URetrieveSkillOverviewWidget::BuildSkillSection(const FGameplayTag& ElementTag) const
{
	const FString ElementName = ElementTagToKorean(ElementTag);
	const UWeaponComponent* Weapon = GetWeaponComponent();

	TArray<FString> Lines;
	if (ElementTag == GetCurrentElementTag())
	{
		Lines.Add(TEXT("● 현재 선택 원소"));
	}
	Lines.Add(TEXT("[흡수] 충전 게이지 1칸 소비"));
	if (ElementTag == RetrieveGameplayTags::Element_Fire)
	{
		Lines.Add(TEXT("  · 10초간 공격력 ×1.25 · 최대 3중첩"));
	}
	else if (ElementTag == RetrieveGameplayTags::Element_Water)
	{
		Lines.Add(TEXT("  · 10초간 받는 피해 ×0.80 · 최대 3중첩"));
	}
	else if (ElementTag == RetrieveGameplayTags::Element_Wind)
	{
		Lines.Add(TEXT("  · 10초간 이동속도 ×1.20 · 최대 3중첩"));
	}
	else
	{
		Lines.Add(TEXT("  · 원소를 선택하면 흡수 효과가 표시됩니다."));
	}
	Lines.Add(FString::Printf(TEXT("  · %s 원소 스택 +1 (60초)"), *ElementName));
	Lines.Add(TEXT(""));
	Lines.Add(TEXT("[버스트] 게이지 3칸 만충 시 발동"));

	const UDataTable* CombinationTable = Cast<UDataTable>(FSoftObjectPath(
		TEXT("/Game/Retrieve/Data/Skill/DT_SkillCombination.DT_SkillCombination")).TryLoad());
	FSkillCombination Combination;
	const bool bFound = Weapon && URetrieveElementUILibrary::GetBurstCombinationByElement(
		CombinationTable, Weapon->GetWeaponDataRef().WeaponTypeTag, ElementTag, Combination);
	if (!bFound)
	{
		Lines.Add(TEXT("  · 현재 무기/원소 조합에 등록된 버스트가 없습니다."));
		return FText::FromString(FString::Join(Lines, TEXT("\n")));
	}

	float TotalMultiplier = 0.f;
	for (const FBurstHitInstance& Hit : Combination.HitSequence)
	{
		TotalMultiplier += Hit.DamageMultiplier;
	}
	const float BaseDamage = Weapon->GetWeaponDataRef().AttackPower * TotalMultiplier;
	Lines.Add(FString::Printf(TEXT("  · %s"), *Combination.DisplayName.ToString()));
	Lines.Add(FString::Printf(TEXT("  · 공격력 합계 배율 ×%.2f · 기본 %.0f 피해"),
		TotalMultiplier, BaseDamage));

	switch (Combination.AttackType)
	{
	case EAttackExecutionType::AreaOfEffect:
		Lines.Add(FString::Printf(TEXT("  · 범위 공격 · 반경 %.0fcm"), Combination.AoeRadius));
		break;
	case EAttackExecutionType::AreaContinuous:
		Lines.Add(FString::Printf(TEXT("  · 지속 범위 · 반경 %.0fcm · %.2f초마다 피해"),
			Combination.AoeRadius, Combination.ContinuousDamageInterval));
		break;
	case EAttackExecutionType::Projectile:
		Lines.Add(FString::Printf(TEXT("  · 투사체 %d회 발사"), Combination.HitSequence.Num()));
		break;
	case EAttackExecutionType::WorldActor:
		Lines.Add(FString::Printf(TEXT("  · 전방 %.0fcm 지점에 범위 효과 생성"), Combination.WorldSpawnDistance));
		break;
	case EAttackExecutionType::Dash:
		Lines.Add(TEXT("  · 돌진/강하 연계 공격"));
		break;
	default:
		Lines.Add(TEXT("  · 근접 범위 공격"));
		break;
	}

	bool bHasStatusEffect = false;
	for (const FBurstHitInstance& Hit : Combination.HitSequence)
	{
		bHasStatusEffect |= !Hit.StatusEffects.IsEmpty();
	}
	if (bHasStatusEffect)
	{
		Lines.Add(FString::Printf(TEXT("  · 적중 시 %s 원소 상태이상 부여"), *ElementName));
	}
	Lines.Add(TEXT("  ※ 표시 피해는 무기 공격력 기준이며 공명·버프·치명타에 따라 변합니다."));
	return FText::FromString(FString::Join(Lines, TEXT("\n")));
}

FText URetrieveSkillOverviewWidget::BuildSetSection() const
{
	const UArmorComponent* Armor = GetArmorComponent();
	const UDataTable* ArmorTable = Armor ? Armor->GetArmorDataTable() : nullptr;
	const UDataTable* SetBonusTable = Cast<UDataTable>(FSoftObjectPath(
		TEXT("/Game/Retrieve/Data/Items/DT_ArmorSetBonus.DT_ArmorSetBonus")).TryLoad());
	if (!Armor || !ArmorTable || !SetBonusTable)
	{
		return FText::FromString(TEXT("착용 중인 세트 없음"));
	}

	TMap<FGameplayTag, int32> PieceCounts;
	for (const FRetrieveEquippedArmorEntry& Entry : Armor->GetEquippedArmorEntries())
	{
		const FRetrieveArmorDataRow* Row = ArmorTable->FindRow<FRetrieveArmorDataRow>(
			Entry.ArmorItemId, TEXT("SkillOverview::SetSection"), false);
		if (Row && Row->ArmorSetTag.IsValid())
		{
			PieceCounts.FindOrAdd(Row->ArmorSetTag)++;
		}
	}

	if (PieceCounts.IsEmpty())
	{
		return FText::FromString(TEXT("착용 중인 세트 없음\n(같은 세트 2부위부터 보너스 발동)"));
	}

	TArray<FString> Lines;
	for (const TPair<FGameplayTag, int32>& Pair : PieceCounts)
	{
		const FRetrieveArmorSetBonusRow* BonusRow = nullptr;
		for (const TPair<FName, uint8*>& RowPair : SetBonusTable->GetRowMap())
		{
			const FRetrieveArmorSetBonusRow* Row =
				reinterpret_cast<const FRetrieveArmorSetBonusRow*>(RowPair.Value);
			if (Row && Row->SetTag == Pair.Key)
			{
				BonusRow = Row;
				break;
			}
		}
		if (!BonusRow) continue;

		if (!Lines.IsEmpty()) Lines.Add(TEXT(""));
		Lines.Add(FString::Printf(TEXT("[%s] %d부위"), *BonusRow->DisplayName.ToString(), Pair.Value));
		if (!BonusRow->Bonus2Desc.IsEmpty())
		{
			Lines.Add(FString::Printf(TEXT("  %s 2세트: %s"),
				Pair.Value >= 2 ? TEXT("●") : TEXT("○"), *BonusRow->Bonus2Desc.ToString()));
		}
		if (!BonusRow->Bonus4Desc.IsEmpty())
		{
			Lines.Add(FString::Printf(TEXT("  %s 4세트: %s"),
				Pair.Value >= 4 ? TEXT("●") : TEXT("○"), *BonusRow->Bonus4Desc.ToString()));
		}
	}
	return FText::FromString(Lines.IsEmpty() ? TEXT("착용 중인 세트 없음") : FString::Join(Lines, TEXT("\n")));
}

FText URetrieveSkillOverviewWidget::BuildResonanceSection() const
{
	const UElementResonanceComponent* Resonance = GetResonanceComponent();
	const UDataTable* Table = Cast<UDataTable>(FSoftObjectPath(
		TEXT("/Game/Retrieve/Data/Skill/DT_ElementResonance.DT_ElementResonance")).TryLoad());
	if (!Resonance || !Table) return FText::GetEmpty();

	const int32 Fire = Resonance->GetElementStackCount(RetrieveGameplayTags::Element_Attune_Fire);
	const int32 Water = Resonance->GetElementStackCount(RetrieveGameplayTags::Element_Attune_Water);
	const int32 Wind = Resonance->GetElementStackCount(RetrieveGameplayTags::Element_Attune_Wind);
	const TArray<FName> ActiveIds = Resonance->GetActiveResonanceIds();

	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("현재 스택 · 불 %d · 물 %d · 바람 %d"), Fire, Water, Wind));
	for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
	{
		const FElementResonanceRow* Row = reinterpret_cast<const FElementResonanceRow*>(Pair.Value);
		if (!Row) continue;

		TArray<FString> Conditions;
		if (Row->RequiredFire > 0) Conditions.Add(FString::Printf(TEXT("불%d"), Row->RequiredFire));
		if (Row->RequiredWater > 0) Conditions.Add(FString::Printf(TEXT("물%d"), Row->RequiredWater));
		if (Row->RequiredWind > 0) Conditions.Add(FString::Printf(TEXT("바람%d"), Row->RequiredWind));

		Lines.Add(FString::Printf(TEXT("%s %s (%s) · %s"),
			ActiveIds.Contains(Pair.Key) ? TEXT("●") : TEXT("○"),
			*Row->DisplayName.ToString(),
			*FString::Join(Conditions, TEXT("+")),
			*Row->Description.ToString()));
	}
	return FText::FromString(FString::Join(Lines, TEXT("\n")));
}

FText URetrieveSkillOverviewWidget::BuildAdvantageSection() const
{
	return FText::FromString(TEXT(
		"패링 성공 · 3초간 전 데미지 +15% · 원소 게이지 충전\n"
		"회피 성공 · 2초간 공격속도 +10% · 이동속도 +10%"));
}

FGameplayTag URetrieveSkillOverviewWidget::GetCurrentElementTag() const
{
	const APawn* Pawn = GetOwningPlayerPawn();
	const ARetrievePlayerState* PS = Pawn ? Pawn->GetPlayerState<ARetrievePlayerState>() : nullptr;
	return PS ? PS->GetCurrentElementTag() : FGameplayTag();
}

FString URetrieveSkillOverviewWidget::ElementTagToKorean(const FGameplayTag& ElementTag)
{
	if (ElementTag == RetrieveGameplayTags::Element_Fire) return TEXT("불");
	if (ElementTag == RetrieveGameplayTags::Element_Water) return TEXT("물");
	if (ElementTag == RetrieveGameplayTags::Element_Wind) return TEXT("바람");
	return TEXT("없음");
}

UWeaponComponent* URetrieveSkillOverviewWidget::GetWeaponComponent() const
{
	APawn* Pawn = GetOwningPlayerPawn();
	return Pawn ? Pawn->FindComponentByClass<UWeaponComponent>() : nullptr;
}

UArmorComponent* URetrieveSkillOverviewWidget::GetArmorComponent() const
{
	APawn* Pawn = GetOwningPlayerPawn();
	return Pawn ? Pawn->FindComponentByClass<UArmorComponent>() : nullptr;
}

UElementResonanceComponent* URetrieveSkillOverviewWidget::GetResonanceComponent() const
{
	APawn* Pawn = GetOwningPlayerPawn();
	return Pawn ? Pawn->FindComponentByClass<UElementResonanceComponent>() : nullptr;
}



