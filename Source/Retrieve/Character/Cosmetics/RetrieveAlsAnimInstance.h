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

protected:
	virtual void NativeInitializeAnimation() override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	/** Class Defaults에서 Tag → 자식 ABP 변수 매핑. 멤버는 자식 ABP가 직접 정의. */
	UPROPERTY(EditDefaultsOnly, Category = "GameplayTags")
	FGameplayTagBlueprintPropertyMap CombatTagMap;
};
