#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Quest/RetrieveQuestDefinition.h"
#include "RetrieveQuestEncounter.generated.h"

class URetrieveQuestObjective;
class URetrieveQuestNPCAppearancePool;
class UAnimMontage;
class UDataTable;
class UStaticMesh;
class USceneComponent;

// ERetrieveQuestPhase 는 Quest/RetrieveQuestDefinition.h 에 정의됨(공용).

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FRetrieveQuestPhaseChangedSignature, ERetrieveQuestPhase, NewPhase);

// FRetrieveQuestItemReward 는 Quest/RetrieveQuestDefinition.h 에 정의됨.

/**
 * 인카운터가 런타임에 스스로 스폰하는 액터 1개의 정의.
 *
 * 자가스폰 하이브리드: 이 정의는 BP 에셋의 CDO에 구워둘 수 있으므로(클래스 참조),
 * 인카운터 액터 1개만 배치하면 NPC/물건 등이 자동 스폰된다.
 * 단, 같은 Role 키가 LinkedActorOverrides(EditInstanceOnly)에 지정돼 있으면
 * 스폰하지 않고 그 액터를 사용한다(레벨에 직접 배치한 액터를 쓰고 싶을 때).
 */
USTRUCT(BlueprintType)
struct FRetrieveQuestSpawnRequest
{
	GENERATED_BODY()

	/** 목표/대화가 참조하는 Role 키(예: "NPC", "QuestItem", "Destination", "Merchant"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Spawn")
	FName Role = NAME_None;

	/** 스폰할 액터 클래스. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Spawn")
	TSubclassOf<AActor> ActorClass;

	/** 인카운터 기준 상대 위치. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Spawn")
	FVector RelativeLocation = FVector::ZeroVector;

	/** 인카운터 기준 상대 회전. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Spawn")
	FRotator RelativeRotation = FRotator::ZeroRotator;

	/**
	 * 이 액터가 보이는 단계들. 비워두면 모든 단계에서 표시.
	 * 예) 퀘스트 물건 = [InProgress], 해방될 상인 = [Completed],
	 *     구출 후 사라지는 포로 = [Offered, InProgress, ReadyToTurnIn].
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Spawn")
	TArray<ERetrieveQuestPhase> VisiblePhases;

	/** true면 스폰 위치에서 지면으로 라인트레이스해 바닥에 맞춘다(캐릭터는 캡슐만큼 자동 상승). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Spawn")
	bool bSnapToGround = true;

	/** true면 인카운터 기준이 아니라 가장 가까운 모닥불 위치를 기준으로 스폰한다(해방 상인 등). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Spawn")
	bool bAtNearestBonfire = false;

	/** true면 스폰 후 이 액터의 스태틱 메시를 QuestItemMeshPool에서 무작위로 교체한다(퀘스트 물건 다양화). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Spawn")
	bool bRandomizeItemMesh = false;
};

/**
 * 데이터 주도 제너릭 퀘스트 인카운터 (자가스폰 하이브리드).
 *
 * 하나의 액터가 목표(URetrieveQuestObjective) 배열 + 4단계 다이얼로그 + 보상 +
 * 스폰 요청을 조합해 모든 유형의 인카운터 퀘스트를 표현한다.
 * 새 유형은 목표 서브클래스만 추가하면 되고, 이 액터/세이브/다이얼로그/보상
 * 파이프라인은 그대로 재사용된다.
 *
 * 배치: 인카운터 액터 1개만 레벨에 두면 SpawnRequests의 NPC/물건이 자동 스폰된다.
 * 특정 Role을 레벨에 직접 배치한 액터로 쓰려면 LinkedActorOverrides에 연결한다.
 * (몬스터 처치형은 별도 ASpawnerBase를 배치하고 SpawnGroupId 태그만 맞춘다)
 */
UCLASS(Blueprintable)
class RETRIEVE_API ARetrieveQuestEncounter : public AActor
{
	GENERATED_BODY()

public:
	ARetrieveQuestEncounter();

	/** 세이브 키. 인스턴스마다 고유해야 한다(자동 생성 툴이 부여). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest")
	FName EncounterId;

	// ── 데이터 주도 정의 (생성 툴이 사용) ───────────────────────────────────
	/** 퀘스트 정의 DataTable(행 구조 FRetrieveQuestDefRow). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Definition")
	TObjectPtr<UDataTable> QuestDefTable;

	/** 위 테이블에서 읽을 행 이름. 설정 시 목표·다이얼로그·보상·스폰을 이 행에서 런타임 구성(인라인 값 대체). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Definition")
	FName QuestDefId;

	/** 자동 스폰할 제너릭 NPC 클래스(RetrieveNPCCharacter 파생). 외형은 풀 인덱스로 적용. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Definition")
	TSubclassOf<AActor> QuestNPCClass;

	/** 자동 스폰할 제너릭 퀘스트 물건(상호작용 회수) 클래스. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Definition")
	TSubclassOf<AActor> QuestItemClass;

	/** NPC 랜덤 외형 풀. 행의 AppearanceIndex로 항목 선택. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Definition")
	TObjectPtr<URetrieveQuestNPCAppearancePool> AppearancePool;

	/** 퀘스트 물건(CollectWorldItem)이 스폰될 때 무작위로 고를 스태틱 메시 풀. 비우면 물건 BP 기본 메시 유지. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Definition")
	TArray<TSoftObjectPtr<UStaticMesh>> QuestItemMeshPool;

	/** 몬스터 처치 목표의 SpawnGroupId를 인스턴스에서 덮어쓴다. 스포너에 이 태그를 달면 연동됨. 비우면 정의값 사용. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest")
	FGameplayTag SpawnGroupIdOverride;

	/** 트래커/로그 표시용 퀘스트 제목. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest")
	FText QuestTitle;

	/**
	 * true면 플레이어가 NPC와 대화해 수락(Offered→InProgress)해야 목표가 시작된다.
	 * false면 배치 즉시 InProgress로 시작한다(구출형: 적이 이미 활성, 대화 수락 없음).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest")
	bool bRequiresOffer = true;

	/** 의뢰/보상 대화를 하는 NPC의 Role 키(SpawnRequests 또는 오버라이드에서 해석). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest")
	FName DialogueNPCRole = TEXT("NPC");

	/** 런타임에 자동 스폰할 액터들(NPC, 물건, 목적지 마커, 상인 등). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest")
	TArray<FRetrieveQuestSpawnRequest> SpawnRequests;

	/** 특정 Role을 레벨에 직접 배치한 액터로 대체(비우면 SpawnRequests로 스폰). */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Retrieve|Quest")
	TMap<FName, TObjectPtr<AActor>> LinkedActorOverrides;

	/**
	 * true면 SpawnRequests(NPC/물건/목적지/상인 자동 스폰)를 전부 무시하고
	 * 레벨에 직접 배치한 액터(LinkedActorOverrides + QuestLinkComponent 태그)만 사용한다.
	 * "절대 자동 생성 안 함"이 필요할 때. false면 배치되지 않은 Role만 자동 스폰(하위호환).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest")
	bool bDisableAutoSpawn = false;

	/** 이 퀘스트의 목표들. 모두 완료되면 턴인 가능. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Retrieve|Quest")
	TArray<TObjectPtr<URetrieveQuestObjective>> Objectives;

	// ── 다이얼로그(단계별로 NPC 인사말 교체) ────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Dialogue")
	TArray<FText> OfferDialogueLines;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Dialogue")
	TArray<FText> InProgressDialogueLines;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Dialogue")
	TArray<FText> ReadyToTurnInDialogueLines;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Dialogue")
	TArray<FText> CompletedDialogueLines;

	// ── 보상 ───────────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Reward", meta = (ClampMin = "0"))
	int32 GoldReward = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Reward")
	TArray<FRetrieveQuestItemReward> ItemRewards;

	// ── 런타임 상태 ─────────────────────────────────────────────────────────
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Retrieve|Quest")
	ERetrieveQuestPhase Phase = ERetrieveQuestPhase::Offered;

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Quest")
	FRetrieveQuestPhaseChangedSignature OnPhaseChanged;

	/** Role 이름으로 (오버라이드 또는 스폰된) 액터를 반환. */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Quest")
	AActor* GetLinkedActor(FName InRole) const;

	/** 새 게임 시작 시 초기 단계로 리셋. */
	void ResetForNewGame();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleDialogueClosed(AActor* PlayerActor);

	UFUNCTION()
	void HandleSaveLoaded();

	/** BP가 단계 변화에 반응하고 싶을 때 override. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Retrieve|Quest")
	void ReceiveQuestPhaseChanged(ERetrieveQuestPhase NewPhase);

private:
	/** QuestDefId가 설정돼 있으면 DataTable 행에서 목표·다이얼로그·보상·스폰을 구성한다. */
	void BuildFromDefinition();
	/** 스폰된 NPC 메시에 외형 풀 항목(인덱스)을 적용한다. */
	void ApplyNPCAppearance(int32 AppearanceIndex);

	/** 현재 단계에 맞춰 NPC 아이들 몽타주(기도 등)를 재생/정지한다. */
	void ApplyNPCIdleMontage();

	void ResolveAndSpawnActors();
	/** 월드에서 QuestLinkComponent 로 이 인카운터에 지정된 배치 액터를 Role별로 등록. */
	void RegisterManuallyLinkedActors();
	void DestroySpawnedActors();
	/** 스폰된 액터의 발/바닥이 지면에 닿도록 지면 트레이스로 재배치. */
	void SnapActorToGround(AActor* Actor);
	/** 인카운터에서 가장 가까운(로드된) 모닥불 액터를 반환. 없으면 nullptr. */
	AActor* FindNearestBonfire() const;
	AActor* GetDialogueNPC() const;

	void HandleObjectiveChanged();

	/**
	 * 현재 단계에 맞춰 목표 마커를 등록/해제한다.
	 *   InProgress    → 미완료 목표마다 마커 1개(위치는 목표가 계산)
	 *   ReadyToTurnIn → 보상을 받을 NPC 마커 1개
	 *   그 외          → 전부 해제
	 */
	void RefreshObjectiveMarkers();
	void ClearObjectiveMarkers();

	/** 이 인카운터가 발급하는 마커 ID의 접두사("<EncounterId>_"). */
	FString GetMarkerIdPrefix() const;

	bool AreAllObjectivesComplete() const;
	bool CanTurnInAll(AActor* Player) const;

	void SetPhase(ERetrieveQuestPhase NewPhase, bool bPersist);
	void ApplyPhase();
	void ActivateObjectives(bool bActivate);
	void UpdateDialogueLines();
	void ApplyActorVisibility();
	bool GrantRewards(AActor* Player);

	ERetrieveQuestPhase GetDefaultPhase() const;

	void RestoreSavedState();
	void PersistState();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	/** Role → 해석된 액터(오버라이드 + 스폰). 런타임. */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<AActor>> RuntimeActors;

	/** 이 인카운터가 스폰한 액터들(정리용). 런타임. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> SpawnedActors;

	/** 레벨에 직접 배치돼(오버라이드/태그) 자동 스폰 대신 사용된 Role들. 외형 덮어쓰기 방지에 사용. */
	TSet<FName> ManuallyLinkedRoles;

	bool bObjectivesActive = false;

	/** BuildFromDefinition이 저장하는 외형 인덱스. 스폰 후 적용. -1이면 미적용. */
	int32 PendingAppearanceIndex = -1;

	/** BuildFromDefinition이 저장하는 NPC 표시 이름. 비어 있으면 미적용. */
	FText PendingNPCDisplayName;

	/** BuildFromDefinition이 저장하는 아이들 몽타주(기도 등)와 재생 단계. */
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> PendingIdleMontage;
	TArray<ERetrieveQuestPhase> PendingIdleMontagePhases;
};
