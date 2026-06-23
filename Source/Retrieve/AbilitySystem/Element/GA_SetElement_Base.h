#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "GA_SetElement_Base.generated.h"

/**
 * 원소 모드 설정 어빌리티의 추상 베이스.
 *
 * 자식 생성자에서 ElementTag와 식별/차단 태그만 설정하면 되고,
 * 모든 원소 태그 클리어 → 해당 원소 태그 부여 → ModeChange 이벤트 브로드캐스트는
 * 베이스의 ActivateAbility가 공통 처리한다.
 */
UCLASS(Abstract)
class RETRIEVE_API UGA_SetElement_Base : public URetrieveGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_SetElement_Base();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	// 자식 생성자가 지정하는 적용 원소 태그 (Element.Fire/Water/Wind)
	FGameplayTag ElementTag;
};
