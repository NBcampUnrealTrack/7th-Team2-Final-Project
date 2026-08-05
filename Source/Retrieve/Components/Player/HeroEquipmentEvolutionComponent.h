#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayTagContainer.h"
#include "HeroEquipmentEvolutionComponent.generated.h"

class UHeroEvolutionConfig;
class UGameplayAbility;
class UGameplayEffect;
class URetrieveAbilitySystemComponent;
class URetrieveSaveSubsystem;

/** 진화 진행 상태 변경 알림(UI 칩용). bSetEquipped=false면 진행바를 숨겨도 된다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FHeroEvolutionProgressSignature,
	int32, Charge, int32, Threshold, bool, bSetEquipped);

/** 진화 완료(잊혀진→전설) 순간 알림. 연출/토스트 트리거용. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHeroEvolutionCompletedSignature);

/**
 * 잊혀진 영웅 장비 → 전설 영웅 장비 진화 컨트롤러(플레이어 폰).
 *
 * 충전과 진화는 '부위 단위'다. 세트 완성을 요구하지 않는다.
 *
 * 1) 착용 중인 잊혀진 부위를 감지(무기 + 방어구 각 슬롯)
 * 2) 흡수/버스트 어빌리티 활성화 시 착용 중인 잊혀진 부위 '각각'에 충전 +1 (세이브 영속)
 * 3) 임계치에 도달한 부위만 전설 대응 아이템으로 스왑(진화)하고 즉시 착용시킨다
 * 4) 나중에 주운 잊혀진 부위는 자기 충전 0부터 다시 쌓아야 진화한다
 *
 * 진화 후 스탯(데미지 2배 / 스킬 범위 2배)은 전설 세트의 방어구 세트 보너스 GE와
 * BurstRadiusMultiplier 어트리뷰트로 처리되며 이 컴포넌트는 스왑만 담당한다.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RETRIEVE_API UHeroEquipmentEvolutionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHeroEquipmentEvolutionComponent();

	/** ASC 준비 후 캐릭터가 호출. 델리게이트 바인딩 + 세이브에서 충전량 복원 + 최초 세트 판정. */
	void InitializeWithAbilitySystem(URetrieveAbilitySystemComponent* InASC);

	/** UnPossess/EndPlay 시 캐릭터가 호출. 델리게이트 해제. */
	void UninitializeFromAbilitySystem();

	/**
	 * UI 진행바용 대표 충전값 — 착용 중인 잊혀진 부위 중 '가장 많이 찬' 값.
	 * 부위마다 값이 다를 수 있는데 진행바는 숫자 하나만 받으므로,
	 * 플레이어가 가장 궁금해할 "다음에 진화할 부위"의 진행도를 보여준다.
	 */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Evolution")
	int32 GetCharge() const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Evolution")
	int32 GetChargeThreshold() const;

	/** 특정 잊혀진 아이템의 충전량. 부위별 진행도를 개별 표시할 때 사용. */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Evolution")
	int32 GetChargeForItem(FName ForgottenItemId) const;

	/** 잊혀진 장비를 한 부위라도 착용 중인지(= 충전이 쌓이는 상태). UI 진행바 표시 조건. */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Evolution")
	bool IsSetEquipped() const { return bSetComplete; }

	/**
	 * 한 부위라도 진화한 적이 있는지(세이브 플래그).
	 * 부위별 충전으로 바뀐 뒤로는 진화를 막는 게이트가 아니며, 연출/기록용으로만 쓴다.
	 */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Evolution")
	bool IsEvolved() const;

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Evolution")
	FHeroEvolutionProgressSignature OnEvolutionProgress;

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Evolution")
	FHeroEvolutionCompletedSignature OnEvolutionCompleted;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 진화 규칙 데이터 에셋. 미지정 시 이 컴포넌트는 아무 동작도 하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Evolution")
	TObjectPtr<UHeroEvolutionConfig> Config;

	/** 전설 세트 착용 시 부여할 보너스 GE(데미지·범위 2배). 방어구 세트태그가 비어 있어도
	 *  이 컴포넌트가 전설 아이템 착용을 직접 감지해 적용하므로 DataTable 세트보너스에 의존하지 않는다.
	 *  생성자에서 GE_Set_LegendaryHero_4pc 기본값 지정. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Evolution")
	TSoftClassPtr<UGameplayEffect> LegendaryBonusEffect;

private:
	UFUNCTION() void OnWeaponChanged(FName WeaponItemId);
	UFUNCTION() void OnArmorChanged(FGameplayTag EquipmentSlotTag, FName ArmorItemId);

	void HandleAbilityActivated(UGameplayAbility* Ability);

	void RecomputeSetComplete();
	// 감지는 ArmorSetTag 대신 Config.ItemEvolutionMap으로 한다(키=잊혀진 ID, 값=전설 ID).

	/** 착용 중인 잊혀진 부위 목록(무기 + 방어구). 충전·진화의 대상 단위. */
	void CollectEquippedForgottenItems(TArray<FName>& OutItemIds) const;

	bool IsLegendarySetComplete() const;   // 착용 무기+방어구가 맵의 '값'인가(2배 세트보너스 판정)
	void RefreshLegendaryBonus();          // 전설 세트 착용 여부에 따라 2배 GE 적용/회수

	/** 착용 중인 잊혀진 부위 각각에 충전을 더한다. 임계치 도달 부위가 생기면 진화를 예약한다. */
	void AddChargeToEquippedItems(int32 Delta);

	/** 임계치를 채운 착용 부위만 전설 장비로 교체한다. */
	void PerformEvolution();
	void BroadcastProgress();

	/** 옛 세이브(전역 충전)를 부위별 충전으로 1회 이관한다. */
	void MigrateLegacyChargeIfNeeded();

	/** 임계치를 채운 착용 부위가 있으면 다음 틱에 진화를 예약한다. */
	void ScheduleEvolutionIfReady();

	bool HasAuthorityToModify() const;
	URetrieveSaveSubsystem* GetSaveSubsystem() const;

	TWeakObjectPtr<URetrieveAbilitySystemComponent> ASC;
	FDelegateHandle AbilityActivatedHandle;
	FActiveGameplayEffectHandle LegendaryBonusHandle;

	bool bSetComplete = false;
	bool bEvolving = false;
	bool bEvolutionScheduled = false;
	bool bDelegatesBound = false;
};
