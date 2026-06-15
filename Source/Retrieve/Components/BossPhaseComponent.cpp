#include "Components/BossPhaseComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Character/RetrieveBossCharacter.h"
#include "Components/RetrieveHealthComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Engine/DataTable.h"
#include "GameplayTags/RetrieveGameplayTags.h"

void UBossPhaseComponent::Initialize(UDataTable* InBossStatsTable, FName InBossStatsRowName,
                                     UDataTable* InMonsterDataTable)
{
	if (!InBossStatsTable || InBossStatsRowName.IsNone())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[%s] BossPhaseComponent::Initialize — BossStatsTable 또는 BossStatsRowName 미설정"),
			*GetName());
		return;
	}

	BossStatsTable   = InBossStatsTable;
	BossStatsRowName = InBossStatsRowName;
	MonsterDataTable = InMonsterDataTable;

	const FBossStatsRow* Row = BossStatsTable->FindRow<FBossStatsRow>(
		BossStatsRowName, TEXT("UBossPhaseComponent::Initialize"));
	if (!Row)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[%s] BossPhaseComponent::Initialize — BossStatsRowName '%s'에 해당하는 행 없음"),
			*GetName(), *BossStatsRowName.ToString());
		return;
	}

	MaxPhases         = FMath::Max(1, Row->PhaseCount);
	Phase2HPThreshold = Row->Phase2HPThreshold;
	Phase3HPThreshold = Row->Phase3HPThreshold;
	CurrentPhase      = 1;

	// HealthComponent OnHealthChanged 구독 (델리게이트 시그니처: float NewHealth)
	if (ARetrieveBossCharacter* Boss = Cast<ARetrieveBossCharacter>(GetOwner()))
	{
		if (URetrieveHealthComponent* HC = Boss->GetHealthComponent())
		{
			HC->OnHealthChanged.AddDynamic(this, &UBossPhaseComponent::OnHealthChanged);
		}
	}
}

void UBossPhaseComponent::OnHealthChanged(float NewHealth)
{
	// 이미 마지막 페이즈이면 체크 불필요
	if (CurrentPhase >= MaxPhases)
	{
		return;
	}

	ARetrieveBossCharacter* Boss = Cast<ARetrieveBossCharacter>(GetOwner());
	if (!Boss)
	{
		return;
	}

	const URetrieveHealthComponent* HealhtCcmp = Boss->GetHealthComponent();
	if (!HealhtCcmp)
	{
		return;
	}

	const float MaxHealth = HealhtCcmp->GetMaxHealth();
	if (MaxHealth <= 0.f)
	{
		return;
	}

	const float Ratio = NewHealth / MaxHealth;

	// 현재 페이즈에 따라 다음 임계값 선택
	const float Threshold = (CurrentPhase == 1) ? Phase2HPThreshold : Phase3HPThreshold;
	if (Ratio <= Threshold)
	{
		TransitionToNextPhase();
	}
}

void UBossPhaseComponent::TransitionToNextPhase()
{
	ARetrieveBossCharacter* Boss = Cast<ARetrieveBossCharacter>(GetOwner());
	if (!Boss) { return; }

	++CurrentPhase;

	// 페이즈 행 이름 규칙: {BossStatsRowName}_Phase{N}
	// 예) "Boss_Fire" → "Boss_Fire_Phase2"
	const FName NewDataRow = FName(
		*(BossStatsRowName.ToString() + FString::Printf(TEXT("_Phase%d"), CurrentPhase)));

	Boss->UpdateMonsterDataRow(NewDataRow);

	// GameplayEvent.Boss.PhaseTransition 전송 → ST가 PhaseTransition 상태 진입
	FGameplayEventData EventData;
	EventData.EventTag   = RetrieveGameplayTags::GameplayEvent_Boss_PhaseTransition;
	EventData.Instigator = Boss;
	EventData.EventMagnitude = CurrentPhase;
	EventData.OptionalObject = ResolvePhaseTransitionMontage();
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		Boss, RetrieveGameplayTags::GameplayEvent_Boss_PhaseTransition, EventData);

	UE_LOG(LogTemp, Log,
		TEXT("[%s] BossPhaseComponent: Phase %d 전환 → DataRow '%s'"),
		*Boss->GetName(), CurrentPhase, *NewDataRow.ToString());
}

UAnimMontage* UBossPhaseComponent::ResolvePhaseTransitionMontage() const
{
	if (!BossStatsTable || BossStatsRowName.IsNone())
	{
		return nullptr;
	}

	const FBossStatsRow* Row = BossStatsTable->FindRow<FBossStatsRow>(
		BossStatsRowName, TEXT("UBossPhaseComponent::ResolvePhaseTransitionMontage"));
	if (!Row)
	{
		return nullptr;
	}

	for (const FBossPhaseTransitionMontageEntry& Entry : Row->PhaseTransitionMontages)
	{
		if (Entry.TargetPhase == CurrentPhase)
		{
			return Entry.Montage.LoadSynchronous();
		}
	}

	return nullptr;
}
