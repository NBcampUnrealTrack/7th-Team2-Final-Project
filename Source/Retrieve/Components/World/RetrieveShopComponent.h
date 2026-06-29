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
};
