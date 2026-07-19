#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "RetrieveQuestObjectives.generated.h"

class ARetrieveQuestEncounter;
class UInventoryComponent;
class URetrieveInteractionResponseComponent;
struct FSpawnGroupClearedPayload;

/** 진행도/완료가 바뀔 때마다 Owner 인카운터가 구독하는 알림. */
DECLARE_MULTICAST_DELEGATE(FOnQuestObjectiveChanged);

/**
 * 퀘스트 목표 1개를 나타내는 인스턴스 UObject.
 *
 * ARetrieveQuestEncounter가 Instanced 배열로 소유하며, 디테일 패널에서
 * 서브클래스를 골라 인라인 편집한다. 새 목표 유형을 추가하려면 이 클래스를
 * 상속해 OnActivate/OnDeactivate/GetProgressText만 구현하면 되고,
 * 인카운터/세이브/다이얼로그/보상 파이프라인은 그대로 재사용된다.
 *
 * 레벨의 특정 액터(스포너·수집 대상·목적지 등)는 직접 참조하지 않고
 * Role 이름으로 Owner->GetLinkedActor(Role)을 통해 해석한다.
 * (인스턴스 UObject가 레벨 액터를 직접 참조하기 어려운 UE 제약을 우회)
 */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, BlueprintType, CollapseCategories)
class RETRIEVE_API URetrieveQuestObjective : public UObject
{
	GENERATED_BODY()

public:
	/** 트래커/다이얼로그에 표시할 목표 설명(예: "늑대 소굴의 몬스터를 처치하세요"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Objective", meta = (MultiLine = true))
	FText Description;

	/** 완료 순간 아이템 획득 토스트로 띄울 메시지. 비우면 토스트 없음(예: "탑을 청소했습니다"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Objective")
	FText CompletionToastMessage;

	/** 진행도/완료 변경 시 Owner 인카운터가 구독. */
	FOnQuestObjectiveChanged OnObjectiveChanged;

	/** 인카운터가 InProgress 진입 시 호출. 이벤트 구독 시작. */
	void ActivateObjective(ARetrieveQuestEncounter* InOwner);

	/** 인카운터가 InProgress를 벗어날 때/EndPlay 시 호출. 구독 해제. */
	void DeactivateObjective();

	bool IsActive() const { return bActive; }
	bool IsComplete() const { return bComplete; }

	/** 트래커에 표시할 진행 텍스트(예: "늑대 2/5"). 기본은 Description. */
	virtual FText GetProgressText() const;

	/** 보상 수령(턴인) 가능한지. 기본 true. AcquireItem 등은 보유량을 재검사. */
	virtual bool CanTurnIn(AActor* Player) const { return true; }

	/** 턴인 확정 시 호출(보상 지급 직전). AcquireItem 등은 아이템을 소모. */
	virtual void OnTurnIn(AActor* Player) {}

	/** 세이브: 완료/진행 상태를 바이트로 직렬화. 기본은 완료 플래그 1바이트. */
	virtual void SerializeProgress(TArray<uint8>& Out) const;
	virtual void RestoreProgress(const TArray<uint8>& In);

protected:
	virtual void OnActivate() {}
	virtual void OnDeactivate() {}

	/** 완료 상태를 설정하고 변경을 브로드캐스트. */
	void SetComplete(bool bNewComplete);
	void BroadcastChanged() { OnObjectiveChanged.Broadcast(); }

	UWorld* GetOwnerWorld() const;
	AActor* ResolveLinkedActor(FName Role) const;
	AActor* GetPlayerPawnSafe() const;
	UInventoryComponent* GetPlayerInventory() const;

	TWeakObjectPtr<ARetrieveQuestEncounter> Owner;
	bool bActive = false;
	bool bComplete = false;
};

/**
 * 목표: 지정한 스폰 그룹의 몬스터를 전부 처치.
 * 대상 스포너(ASpawnerBase)의 SpawnGroupId와 동일하게 설정한다.
 */
UCLASS(meta = (DisplayName = "목표: 몬스터 처치 (스폰 그룹)"))
class RETRIEVE_API UQuestObjective_ClearSpawnGroup : public URetrieveQuestObjective
{
	GENERATED_BODY()

public:
	/** 완료를 유발하는 스폰 그룹. 대상 스포너의 SpawnGroupId와 동일하게 설정. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Objective")
	FGameplayTag SpawnGroupId;

protected:
	virtual void OnActivate() override;
	virtual void OnDeactivate() override;
	virtual FText GetProgressText() const override;

private:
	void HandleSpawnGroupCleared(FGameplayTag Channel, const FSpawnGroupClearedPayload& Payload);
	FGameplayMessageListenerHandle ClearedHandle;
};

/**
 * 목표: 월드에 배치된 특정 물건 액터와 상호작용해 회수.
 * 대상 액터는 인카운터 LinkedActors[ItemActorRole]에 연결하고,
 * 그 액터에 URetrieveInteractionResponseComponent가 있어야 한다.
 */
UCLASS(meta = (DisplayName = "목표: 물건 찾아오기 (월드 오브젝트)"))
class RETRIEVE_API UQuestObjective_CollectWorldItem : public URetrieveQuestObjective
{
	GENERATED_BODY()

public:
	/** 인카운터 LinkedActors에서 회수 대상 액터를 찾을 Role 키. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Objective")
	FName ItemActorRole = TEXT("QuestItem");

protected:
	virtual void OnActivate() override;
	virtual void OnDeactivate() override;
	virtual FText GetProgressText() const override;

private:
	UFUNCTION()
	void HandleItemInteracted(AActor* InteractionInstigator);

	TWeakObjectPtr<URetrieveInteractionResponseComponent> BoundResponse;
};

/**
 * 목표: 지정 위치(연결된 목적지 액터 주변)에 도달.
 * 폴링 방식이라 별도 트리거 볼륨 없이 아무 액터나 목적지로 지정 가능.
 */
UCLASS(meta = (DisplayName = "목표: 위치 도달"))
class RETRIEVE_API UQuestObjective_ReachLocation : public URetrieveQuestObjective
{
	GENERATED_BODY()

public:
	/** 인카운터 LinkedActors에서 목적지 액터를 찾을 Role 키. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Objective")
	FName DestinationRole = TEXT("Destination");

	/** 도달로 간주할 반경(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Objective", meta = (ClampMin = "50.0"))
	float AcceptanceRadius = 350.f;

protected:
	virtual void OnActivate() override;
	virtual void OnDeactivate() override;

private:
	void PollDistance();
	FTimerHandle PollTimerHandle;
};

/**
 * 목표: 특정 아이템을 N개 획득.
 *
 * 몬스터 드랍 연동: 대상 몬스터의 DropTable에 ItemId 행을 추가하면,
 * 처치 시 DropComponent가 인벤토리로 지급 → OnItemAdded 감지로 진행도 상승.
 * (자동 생성 툴이 드랍 행 주입을 담당) 일반 수집 픽업도 동일하게 감지된다.
 */
UCLASS(meta = (DisplayName = "목표: 아이템 획득 (몬스터 드랍/수집)"))
class RETRIEVE_API UQuestObjective_AcquireItem : public URetrieveQuestObjective
{
	GENERATED_BODY()

public:
	/** 획득 대상 아이템 ID(DataTable RowName). 몬스터 DropTable 행과 일치시킨다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Objective")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Objective", meta = (Categories = "Item"))
	FGameplayTag ItemCategoryTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Objective", meta = (ClampMin = "1"))
	int32 RequiredCount = 1;

	/** true면 턴인 시 획득한 퀘스트 아이템을 인벤토리에서 소모. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Objective")
	bool bConsumeOnTurnIn = true;

protected:
	virtual void OnActivate() override;
	virtual void OnDeactivate() override;
	virtual FText GetProgressText() const override;
	virtual bool CanTurnIn(AActor* Player) const override;
	virtual void OnTurnIn(AActor* Player) override;

private:
	UFUNCTION()
	void HandleItemAdded(FName InItemId, FGameplayTag Category, int32 Quantity);

	void Recount();

	int32 CurrentCount = 0;
	TWeakObjectPtr<UInventoryComponent> BoundInventory;
};
