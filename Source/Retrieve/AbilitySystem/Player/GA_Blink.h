#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "GA_Blink.generated.h"

/** 점멸 — 스태프 전용 이동기(대시 대체). 일정 거리 순간이동, 무적 없음. */
UCLASS()
class RETRIEVE_API UGA_Blink : public URetrieveGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Blink();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	FVector ResolveBlinkDirection(const FGameplayAbilityActorInfo* ActorInfo) const;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Blink", meta = (ClampMin = "0.0"))
	float BlinkDistance = 600.f;

	// 도착지에 적이 겹칠 때 그 앞에 띄울 여유
	UPROPERTY(EditDefaultsOnly, Category = "Blink", meta = (ClampMin = "0.0"))
	float EnemyStandoffGap = 50.f;

	UPROPERTY(EditDefaultsOnly, Category = "Blink")
	bool bDebugDraw = false;
};
