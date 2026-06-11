#include "Character/RetrieveBossCharacter.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Components/BossPhaseComponent.h"
#include "Components/EnemyCombatComponent.h"
#include "Components/NormalMonsterHealthBarComponent.h"
#include "Components/RetrieveHealthComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Engine/DataTable.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayMessages/RetrieveGameplayMessageTypes.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"

ARetrieveBossCharacter::ARetrieveBossCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BossPhaseComponent = CreateDefaultSubobject<UBossPhaseComponent>(TEXT("BossPhaseComponent"));

	if (NormalHealthBarComponent)
	{
		NormalHealthBarComponent->SetHealthBarEnabled(false);
	}
}

void ARetrieveBossCharacter::InitializeComponents()
{
	// EnemyCombatComponent / PatternCounterComponent / DropComponent 초기화
	Super::InitializeComponents();

	// BossPhaseComponent 초기화 (DT_BossStats 읽기 + HealthComponent 구독)
	if (BossPhaseComponent)
	{
		BossPhaseComponent->Initialize(BossStatsTable, BossStatsRowName, MonsterDataTable);
	}
}

void ARetrieveBossCharacter::HandleDeathStarted(AActor* OwningActor)
{
	ARetrieveCombatCharacter::HandleDeathStarted(OwningActor);
	
	if (OwnedASC)
	{
		OwnedASC->AddLooseGameplayTag(RetrieveGameplayTags::State_Enemy_Dead);
	}

	if (!HasAuthority())
	{
		return;
	}
	
	// 2. Channel.Monster.Died 브로드캐스트 (퀘스트·킬카운터 등 공통 리스너 처리)
	const URetrieveHealthComponent* HC = GetHealthComponent();
	FMonsterDiedPayload DiedPayload;
	DiedPayload.DeadActor     = this;
	DiedPayload.DeathLocation = GetActorLocation();
	DiedPayload.Killer        = HC ? HC->GetLastDamageInstigator() : nullptr;
	DiedPayload.DamageCauser  = HC ? HC->GetLastDamageCauser()    : nullptr;
	DiedPayload.MonsterDataRow = MonsterDataRowName;

	UGameplayMessageSubsystem::Get(this).BroadcastMessage(
		RetrieveGameplayTags::Channel_Monster_Died, DiedPayload);

	// 3. GameplayEvent.Boss.Die 전송 → GA_Boss_Die 트리거 (사망 몽타주 + 코어 드랍 hook)
	FGameplayEventData EventData;
	EventData.EventTag   = RetrieveGameplayTags::GameplayEvent_Boss_Die;
	EventData.Instigator = this;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		this, RetrieveGameplayTags::GameplayEvent_Boss_Die, EventData);

	
	const FBossStatsRow* Row = GetBossStatsRow();
	if (!Row)
	{
		return;
	}

	if (Row->UnlockElementTag.IsValid())
	{
		// 가디언: Channel.Quest.GuardianDefeated → 속성 해금 파이프라인
		FRetrieveGuardianDefeatedPayload GuardianPayload;
		GuardianPayload.GuardianElement = Row->UnlockElementTag;
		UGameplayMessageSubsystem::Get(this).BroadcastMessage(
			RetrieveGameplayTags::Channel_Quest_GuardianDefeated, GuardianPayload);
	}
	else
	{
		// 여왕: Channel.Game.QueenDefeated → 엔딩 선택 트리거
		UGameplayMessageSubsystem::Get(this).BroadcastMessage(
			RetrieveGameplayTags::Channel_Game_QueenDefeated, DiedPayload);
	}
}

void ARetrieveBossCharacter::UpdateMonsterDataRow(FName NewRow)
{
	// 보호된 부모 필드에 직접 접근 가능 (같은 상속 계층)
	MonsterDataRowName = NewRow;

	// EnemyCombatComponent 패턴 슬롯 재초기화
	if (!MonsterDataTable || MonsterDataRowName.IsNone()) { return; }

	const FMonsterDataRow* Row = MonsterDataTable->FindRow<FMonsterDataRow>(
		MonsterDataRowName, TEXT("ARetrieveBossCharacter::UpdateMonsterDataRow"));
	if (!Row) { return; }

	if (EnemyCombatComponent)
	{
		EnemyCombatComponent->Initialize(PatternTable, Row->PatternSlots);
	}
}

FGameplayTag ARetrieveBossCharacter::GetUnlockElementTag() const
{
	const FBossStatsRow* Row = GetBossStatsRow();
	return Row ? Row->UnlockElementTag : FGameplayTag();
}

const FBossStatsRow* ARetrieveBossCharacter::GetBossStatsRow() const
{
	if (!BossStatsTable || BossStatsRowName.IsNone()) { return nullptr; }
	return BossStatsTable->FindRow<FBossStatsRow>(
		BossStatsRowName, TEXT("ARetrieveBossCharacter::GetBossStatsRow"));
}
