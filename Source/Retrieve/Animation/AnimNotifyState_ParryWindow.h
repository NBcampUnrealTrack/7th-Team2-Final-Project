
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_ParryWindow.generated.h"

class URetrieveGameplayAbility;

/**
 * 몽타주 기반 ParryWindow.
 *
 * 이 NotifyState는 특정 Ability 클래스(예: UGA_GuardAttack)를 직접 알지 않는다.
 * 대신 현재 활성화된 URetrieveGameplayAbility 인스턴스들에게 공용 hook을 호출한다.
 *
 * 이렇게 둔 이유:
 * - 지금은 GuardAttack의 bCanStartParry만 사용한다.
 * - 추후 일반 공격, SprintAttack, HeavyAttack에도 bCanStartParry를 추가하면
 *   이 NotifyState를 그대로 꽂고 해당 Ability의 hook만 override하면 된다.
 *
 * 공격 판정과의 관계:
 * - AttackImpact NotifyState: 방패/무기 trace로 적에게 데미지를 주는 공격 window.
 * - ParryWindow NotifyState: 내가 맞았을 때 State.Player.Parrying으로 패리를 성립시키는 방어 window.
 * 두 window는 일부 겹칠 수 있지만 같은 개념이 아니다.
 */
UCLASS(DisplayName = "Parry Window")
class RETRIEVE_API UAnimNotifyState_ParryWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

private:
	void ForEachActiveRetrieveAbility(
		const USkeletalMeshComponent* MeshComp,
		TFunctionRef<void(URetrieveGameplayAbility&)> Func) const;
};
