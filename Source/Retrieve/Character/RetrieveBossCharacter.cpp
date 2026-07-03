#include "Character/RetrieveBossCharacter.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Components/Enemy/BossPhaseComponent.h"
#include "Components/Enemy/EnemyCombatComponent.h"
#include "Components/Enemy/EnemyPoiseComponent.h"
#include "Components/Enemy/NormalMonsterHealthBarComponent.h"
#include "Components/Enemy/PatternCounterComponent.h"
#include "Components/Combat/RetrieveHealthComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Engine/DataTable.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Messaging/GameplayMessages/RetrieveGameplayMessageTypes.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Components/World/RetrieveMapIconComponent.h"
#include "Data/RetrieveMapIconRegistry.h"

ARetrieveBossCharacter::ARetrieveBossCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BossPhaseComponent = CreateDefaultSubobject<UBossPhaseComponent>(TEXT("BossPhaseComponent"));

	if (NormalHealthBarComponent)
	{
		NormalHealthBarComponent->SetHealthBarEnabled(false);
	}

	// 보스는 미니맵·나침반에서 Boss 아이콘으로 표시 (부모가 만든 컴포넌트의 타입만 변경).
	if (MapIconComponent)
	{
		MapIconComponent->IconType = ERetrieveMapIconType::Boss;
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

	// ARetrieveEnemyCharacter::HandleDeathStarted를 거치지 않으므로 여기서도 직접 처리.
	if (MapIconComponent)
	{
		MapIconComponent->bShowOnMinimap = false;
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
	if (!Row)
	{
		return;
	}


	if (EnemyCombatComponent)
	{
		EnemyCombatComponent->Initialize(PatternTable, Row->PatternSlots);
	}

	if (PatternCounterComponent)
	{
		PatternCounterComponent->SetGroggyCooldown(Row->GroggyCooldown);
	}

	if (EnemyPoiseComponent)
	{
		EnemyPoiseComponent->InitializeFromMonsterData(*Row, true);
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
