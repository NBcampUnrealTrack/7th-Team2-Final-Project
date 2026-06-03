#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/RetrieveAttributeSet.h"
#include "CombatAttributeSet.generated.h"

/**
 * 모든 전투 가능한 캐릭터(소버린, 적, 보스)가 공유합니다.
 * 무기 및 스킬 데미지는 어트리뷰트셋 외부에서 계산됩니다.
 */

UCLASS()
class RETRIEVE_API UCombatAttributeSet : public URetrieveAttributeSet
{
	GENERATED_BODY()

public:
	UCombatAttributeSet();

	/** MoveSpeed Attribute의 기준값(cm/s). 이 값이 ALS DA 속도 그대로(=배율 1.0)에 대응합니다. */
	static constexpr float ReferenceMoveSpeed = 600.f;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, MaxHealth)
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AttackPower)
	FGameplayAttributeData AttackPower;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, AttackPower)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MoveSpeed)
	FGameplayAttributeData MoveSpeed;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, MoveSpeed)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_IncomingDamageMultiplier)
	FGameplayAttributeData IncomingDamageMultiplier;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, IncomingDamageMultiplier)

	UPROPERTY(BlueprintReadOnly)
	FGameplayAttributeData IncomingDamage;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, IncomingDamage)

	UPROPERTY(BlueprintReadOnly)
	FGameplayAttributeData IncomingHealing;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, IncomingHealing)
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_GuardDamageReduction)
	FGameplayAttributeData GuardDamageReduction;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, GuardDamageReduction)

private:
	UFUNCTION() void OnRep_Health(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_AttackPower(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MoveSpeed(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_IncomingDamageMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_GuardDamageReduction(const FGameplayAttributeData& OldValue);

	// 방어 상태(State.Player.Guarding / State.Player.Shielded)에 따른 감쇠/가드브레이크 분기 처리, 우선순위: Guarding > Shielded (누적 없음)
	float HandleIncomingDamage_Guard(const FGameplayEffectModCallbackData& Data, float RawDamage, const FGameplayTagContainer& SpecTags);
	// 데미지 적용 후 카메라/플로터/리액션 시스템에 알릴 이벤트 발행
	void BroadcastHitEvent(const struct FGameplayEffectModCallbackData& Data, float DamageDone) const;
};
