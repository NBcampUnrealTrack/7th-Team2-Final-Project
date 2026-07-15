#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "RetrieveShopComponent.generated.h"

class URetrieveShopDefinitionAsset;
class APlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FRetrieveShopOpenRequestSignature,
	URetrieveShopDefinitionAsset*, ShopDefinition,
	APlayerController*, InstigatorPC);

/** NPC 액터에 붙는 상점 컴포넌트.
 *  RetrieveInteractionResponseComponent.OnApplied와 연동하거나
 *  BP에서 직접 OpenShop()을 호출한다. */
UCLASS(ClassGroup = (Retrieve), meta = (BlueprintSpawnableComponent))
class RETRIEVE_API URetrieveShopComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URetrieveShopComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** InteractionResponseComponent.OnApplied 또는 BP 이벤트에서 호출 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Shop")
	void OpenShop(AActor* InstigatorActor);

	/** 화톳불 사용 시 또는 수동 호출 시 순환 재고를 다시 뽑습니다. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Shop")
	void RollRotatingStock();

	// ── 재고 (유한 재고 상점, 세이브 지속) ──────────────────────────────────────
	/** 이 상점에서 RowName 슬롯의 남은 재고. 무한 재고이면 -1을 반환한다. */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Shop")
	int32 GetRemainingStock(FName RowName) const;

	/** 구매 성공 시 재고를 Quantity만큼 차감하고 세이브에 반영한다.
	 *  무한 재고이면 아무것도 하지 않고 true. 재고 부족이면 false. */
	bool ConsumeStock(FName RowName, int32 Quantity);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Shop")
	TObjectPtr<URetrieveShopDefinitionAsset> ShopDefinition;

	/** 현재 진열된 순환 재고 행 이름 목록. ShopPanelWidget이 읽습니다. */
	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Shop")
	TArray<FName> CachedRotatingRows;

	/** PlayerController가 구독해 WBP_ShopPanel을 생성/표시한다 */
	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Shop")
	FRetrieveShopOpenRequestSignature OnShopOpenRequested;

private:
	FGameplayMessageListenerHandle RestListenerHandle;

	// ── 재고 내부 상태 ──────────────────────────────────────────────────────────
	/** 이 상점의 식별자(정의 에셋 이름). 세이브 키로 사용. */
	FName GetShopId() const;

	/** 세이브에서 이 상점의 소진 재고를 세션 최초 1회 로드한다. */
	void EnsureStockInitialized();

	/** 현재 RemainingStock 맵을 세이브에 기록한다. */
	void PersistStock() const;

	/** 정의 에셋(메인/순환 테이블)에서 RowName 행을 찾는다. */
	const struct FRetrieveShopItemRow* FindDefinitionRow(FName RowName) const;

	/** RowName → 남은 재고. 유한 재고 행만 담긴다(무한 재고는 미포함). */
	TMap<FName, int32> RemainingStockMap;

	bool bStockInitialized = false;
};
