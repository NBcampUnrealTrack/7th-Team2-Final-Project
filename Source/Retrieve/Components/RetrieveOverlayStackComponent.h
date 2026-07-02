#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "RetrieveOverlayStackComponent.generated.h"

class URetrieveAbilitySystemComponent;
class URetrieveOverlayMaterialMapDataAsset;
class UMeshComponent;
class UMaterialInterface;

/**
 * 소유 캐릭터의 SkeletalMeshComponent 오버레이 머티리얼 슬롯을 단일 소유자로 관리한다.
 * OverlayMap에 등록된 태그들 중 활성(카운트>0)인 것들 중
 * (Priority DESC, TagName ASC)로 최상위 엔트리의 머티리얼을 슬롯에 반영한다.
 */
UCLASS(ClassGroup = "Retrieve", meta = (BlueprintSpawnableComponent))
class RETRIEVE_API URetrieveOverlayStackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URetrieveOverlayStackComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/** 태그 → 오버레이 머티리얼 + 우선순위 매핑 DataAsset. 에디터에서 할당. */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Overlay")
	TObjectPtr<URetrieveOverlayMaterialMapDataAsset> OverlayMap;

private:
	void OnAbilitySystemReady();
	void OnTrackedTagChanged(const FGameplayTag Tag, int32 NewCount);

	void Recalculate();

	void ApplyOverlayToOwnerMeshes(UMaterialInterface* Material) const;
	
	TWeakObjectPtr<URetrieveAbilitySystemComponent> BoundASC;
	TArray<TPair<FGameplayTag, FDelegateHandle>> TagBindings;
	TWeakObjectPtr<UMaterialInterface> CurrentMaterial;
};
