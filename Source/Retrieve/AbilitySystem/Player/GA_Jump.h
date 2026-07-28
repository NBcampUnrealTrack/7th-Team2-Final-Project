#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "GA_Jump.generated.h"

/**
 * 기본 점프 어빌리티.
 * ActivateAbility에서 ACharacter::Jump()를 호출해 점프를 시작하고 즉시 종료한다.
 * 점프 궤적/물리는 CharacterMovement가 처리한다(접지 여부 등은 CanJump가 판단).
 *
 * 점프는 '공격'이 아니다 — Ability.Type.Attack 자산태그도, CancelAbilitiesWithTag(Attack)도 두지 않는다.
 * (점프로 공격을 끊는 동작이 필요하면 Attack 태그가 아닌 별도 메커니즘으로 처리)
 */
UCLASS()
class RETRIEVE_API UGA_Jump : public URetrieveGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Jump();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};