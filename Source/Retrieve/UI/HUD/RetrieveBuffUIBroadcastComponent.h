#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActiveGameplayEffectHandle.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameplayTagContainer.h"
#include "RetrieveBuffUIBroadcastComponent.generated.h"

class UAbilitySystemComponent;
class UDataTable;
struct FActiveGameplayEffect;
struct FGameplayEffectSpec;

/**
 * 플레이어 Character에 추가하는 컴포넌트.
 * ASC에서 GE 추가/제거를 감시 → DT_BuffUIDefinitions 조회 → 버프 바 메시지 발행.
 * 새 버프 추가 시 코드 수정 불필요 — DataTable 행 추가만으로 동작.
 *
 * [설정 방법]
 * 1. 플레이어 BP의 Components 패널에 이 컴포넌트를 추가
 * 2. Details > BuffUI > BuffUITable → DT_BuffUIDefinitions 지정
 * 3. Details > BuffUI > WatchTagPrefix → "UI.Buff" 지정
 */
UCLASS(ClassGroup = "Retrieve|UI", meta = (BlueprintSpawnableComponent))
class RETRIEVE_API URetrieveBuffUIBroadcastComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URetrieveBuffUIBroadcastComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * GE 없이 수동으로 버프를 발행한다 (버스트 연출, Instant GE 등).
	 * DataTable에 해당 태그 행이 있어야 한다.
	 * DurationOverride > 0 이면 해당 값을, 0 이면 DataTable의 DurationOverride를 사용한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI|Buff")
	void BroadcastBuffManual(
		FGameplayTag BuffUITag,
		float DurationOverride = 0.f,
		TSubclassOf<UGameplayEffect> SourceEffect = nullptr);

	/** 수동으로 버프를 제거한다. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI|Buff")
	void BroadcastBuffRemove(FGameplayTag BuffUITag);

	void RefreshAbilitySystemBinding();

private:
	/** BP Details에서 DT_BuffUIDefinitions 에셋을 지정한다. */
	UPROPERTY(EditDefaultsOnly, Category = "BuffUI")
	TObjectPtr<UDataTable> BuffUITable;

	/**
	 * 이 prefix를 포함하는 GE Asset Tag만 감시한다.
	 * 에디터 Details에서 "UI.Buff" 를 지정한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "BuffUI")
	FGameplayTag WatchTagPrefix;

	// GE Handle → BuffId 매핑 (GE 제거 시 BuffId 복원용)
	TMap<FActiveGameplayEffectHandle, FGameplayTag> HandleToBuffId;
	TMap<FGameplayTag, FRetrieveBuffUIRow> BuiltInRows;
	TWeakObjectPtr<UAbilitySystemComponent> CachedASC;

	FDelegateHandle OnGEAddedHandle;
	FDelegateHandle OnGERemovedHandle;

	void BindToASC(UAbilitySystemComponent* ASC);
	void UnbindFromASC();

	void OnGEAdded(UAbilitySystemComponent* ASC,
	               const FGameplayEffectSpec& Spec,
	               FActiveGameplayEffectHandle Handle);

	void OnGERemoved(const FActiveGameplayEffect& ActiveGE);

	void InitBuiltInRows();
	void BroadcastApply(const FRetrieveBuffUIRow& Row, float Duration, TSubclassOf<UGameplayEffect> SourceEffect = nullptr);
	const FRetrieveBuffUIRow* FindRow(FGameplayTag BuffUITag) const;
};
