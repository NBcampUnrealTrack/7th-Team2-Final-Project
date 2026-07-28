#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "GA_Blink.generated.h"

class ACharacter;
class UAbilityTask_ApplyRootMotionMoveToForce;

/**
 * 점멸 — 스태프 전용 이동기(대시 대체).
 * 시작 시 메시를 숨겨 이동(정면+낙하) 과정을 감추고, 도착(착지) 시 다시 보인다(체공 애니도 숨겨짐).
 * 정면 고속 RootMotion 이동(Walking: 지형 타고 오르막 등반, 물리 충돌이 벽·적 앞 정지) 후,
 * 내리막에서 공중에 뜨면 중력을 강하게 키워 바닥까지 고속 낙하. 무적 없음.
 */
UCLASS()
class RETRIEVE_API UGA_Blink : public URetrieveGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Blink();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	FVector ResolveBlinkDirection(const FGameplayAbilityActorInfo* ActorInfo) const;
	void FinishBlink(ACharacter* Character);

	UFUNCTION() void OnForwardFinished();

private:
	UPROPERTY(EditDefaultsOnly, Category = "Blink", meta = (ClampMin = "0.0"))
	float BlinkDistance = 600.f;

	UPROPERTY(EditDefaultsOnly, Category = "Blink", meta = (ClampMin = "1.0"))
	float DashSpeed = 6000.f;

	// 내리막 고속 낙하용 중력 배율. 클수록 빠르게 붙는다.
	UPROPERTY(EditDefaultsOnly, Category = "Blink", meta = (ClampMin = "1.0"))
	float BlinkFallGravityScale = 12.f;

	// 착지가 안 잡혀도 이 시간(초) 뒤 은닉/중력을 강제 복원.
	UPROPERTY(EditDefaultsOnly, Category = "Blink", meta = (ClampMin = "0.1"))
	float BlinkFallMaxDuration = 2.0f;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_ApplyRootMotionMoveToForce> DashTask;

	// 낙하로 넘어갔는지(내리막). true면 종료 후에도 캐릭터가 착지할 때까지 숨김을 유지한다.
	bool bFallHandoff = false;
};
