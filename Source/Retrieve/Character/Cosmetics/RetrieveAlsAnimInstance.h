#pragma once

#include "CoreMinimal.h"
#include "AlsAnimationInstance.h"
#include "GameplayEffectTypes.h"

#include "RetrieveAlsAnimInstance.generated.h"

class UAbilitySystemComponent;

/**
 * Sovereign 등 ALS 가지 캐릭터의 메인 AnimInstance.
 * UAlsAnimationInstance의 View / Locomotion / Foot IK 등을 모두 그대로 사용하며,
 * GAS 태그 → bool 멤버 자동 동기화를 위한 PropertyMap을 추가합니다.
 *
 * 초기화는 이중 경로 (둘 중 빠른 쪽이 처리):
 *   경로 A: NativeInitializeAnimation에서 OwningActor → ASC 조회 후 자가 초기화
 *   경로 B: URetrieveAbilitySystemComponent::InitAbilityActorInfo에서 외부 호출
 */
UCLASS()
class RETRIEVE_API URetrieveAlsAnimInstance : public UAlsAnimationInstance
{
	GENERATED_BODY()

public:
	/** ASC가 준비된 시점에 PropertyMap을 바인딩. 두 경로 어느 쪽에서 호출돼도 안전. */
	virtual void InitializeWithAbilitySystem(UAbilitySystemComponent* ASC);

	// 스탠스 접근자. C++는 '선언'만 하고 구현은 자식 ABP가 담당한다(= 프로퍼티 맵이 채운 BP 변수를 반환).
	// 레이어 ABP 상태기가 GetRetrieveParent로 '캐스트 없이' thread-safe 호출 → 클래스가 달라도 BP 값을 읽는다.
	// (FGameplayTagBlueprintPropertyMap은 BP 변수만 채울 수 있어 C++ 멤버는 불가 → 이 우회로 사용)
	UFUNCTION(BlueprintImplementableEvent, BlueprintPure, Category = "Retrieve|Stance", meta = (BlueprintThreadSafe))
	bool IsWeaponSheathed() const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintPure, Category = "Retrieve|Stance", meta = (BlueprintThreadSafe))
	bool IsInCombat() const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintPure, Category = "Retrieve|Stance", meta = (BlueprintThreadSafe))
	bool IsGuarding() const;

protected:
	virtual void NativeInitializeAnimation() override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	/** Class Defaults에서 Tag → 자식 ABP의 BP 변수 매핑. (맵은 BP 변수만 대상 — C++ 멤버는 불가) */
	UPROPERTY(EditDefaultsOnly, Category = "GameplayTags")
	FGameplayTagBlueprintPropertyMap CombatTagMap;
};
