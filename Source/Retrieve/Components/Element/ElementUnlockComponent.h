#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "ElementUnlockComponent.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class URetrieveSaveSubsystem;
struct FGameplayEventData;

/** 원소가 새로 해방될 때 브로드캐스트. "원소모드 강화" 등 다운스트림 연결 훅. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnElementUnlocked, FGameplayTag, ElementTag);

/**
 * 가디언 코어 흡수(GameplayEvent.Core.Absorb)를 구독하여 해당 원소를 해방하고,
 * 해방 목록을 Persistent World State(RetrieveSave_WorldState 슬롯)에 영속 저장한다.
 *
 * 부착: ASovereignCharacter (플레이어 폰).
 * 영속: URetrieveSaveSubsystem이 WorldState 슬롯에 즉시 자동 저장 / 로드.
 *       폰이 재생성돼도 InitializeWithAbilitySystem에서 WorldState로부터 복원됨.
 */
UCLASS(ClassGroup = "Retrieve", meta = (BlueprintSpawnableComponent))
class RETRIEVE_API UElementUnlockComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UElementUnlockComponent();

	/** ASovereignCharacter::InitializeAbilitySystem에서 호출.
	 *  ASC의 GameplayEvent.Core.Absorb 구독 + WorldState에서 해방 상태 복원. */
	void InitializeWithAbilitySystem(UAbilitySystemComponent* InASC);

	/** 구독 해제. UnPossessed / EndPlay에서 호출. */
	void UninitializeFromAbilitySystem();

	/** 루멘 각인 완료 표시(단순 상태 플래그). 영속 저장 반영. 해방 동작을 게이팅하지 않음. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|ElementUnlock")
	void InitializeByLumenEngrave();

	/** 원소 해방. 이미 해방된 원소면 무시. 영속 저장 반영 후 OnElementUnlocked 브로드캐스트. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|ElementUnlock")
	void UnlockElement(FGameplayTag ElementTag);

	UFUNCTION(BlueprintPure, Category = "Retrieve|ElementUnlock")
	bool IsElementUnlocked(FGameplayTag ElementTag) const;

	/** AllElements(기본 Fire/Water/Wind)가 모두 해방됐는지. */
	UFUNCTION(BlueprintPure, Category = "Retrieve|ElementUnlock")
	bool AreAllElementsUnlocked() const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|ElementUnlock")
	FGameplayTagContainer GetUnlockedElements() const { return UnlockedElements; }

	UFUNCTION(BlueprintPure, Category = "Retrieve|ElementUnlock")
	bool IsLumenEngraved() const { return bLumenEngraved; }

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|ElementUnlock")
	FOnElementUnlocked OnElementUnlocked;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/** GameplayEvent.Core.Absorb 콜백. Payload->InstigatorTags[0]를 해방 원소로 사용. */
	void HandleCoreAbsorb(const FGameplayEventData* Payload);

	/** GameplayEvent.Element.ModeChange 콜백. 원소 모드 전환 시 각성 버프 재평가. */
	void HandleElementModeChanged(const FGameplayEventData* Payload);

	/** 현재 원소 모드 && 해당 원소 해금이면 각성 버프 GE를 부여, 그 외에는 제거. 호스트 전용. */
	void RefreshAwakeningEffect();

	/** WorldState(SaveSubsystem)로부터 해방 목록 / 각인 상태 복원. 호스트 전용. */
	void LoadFromPersistentState();

	URetrieveSaveSubsystem* GetSaveSubsystem() const;

	/** AreAllElementsUnlocked 판정 기준 집합. 생성자에서 Fire/Water/Wind로 초기화. */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|ElementUnlock")
	FGameplayTagContainer AllElements;

	/** 원소 각성 버프: 해당 원소를 해금하고 그 모드일 때 부여되는 무한 지속 GE.
	 *  예) Element.Wind → GE_WindAwakening (AttackSpeedMultiplier +0.3). 미설정 원소는 버프 없음. */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|ElementUnlock")
	TMap<FGameplayTag, TSubclassOf<UGameplayEffect>> ElementAwakeningEffects;

	/** 현재 부여 중인 각성 버프 핸들. 모드 전환 / 정리 시 제거에 사용. */
	FActiveGameplayEffectHandle ActiveAwakeningHandle;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Retrieve|ElementUnlock")
	FGameplayTagContainer UnlockedElements;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Retrieve|ElementUnlock")
	bool bLumenEngraved = false;

	TWeakObjectPtr<UAbilitySystemComponent> ASC;
};
