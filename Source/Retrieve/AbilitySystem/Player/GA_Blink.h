#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "Engine/EngineTypes.h"
#include "GA_Blink.generated.h"

class UAnimMontage;
class ACharacter;
class UAbilityTask_ApplyRootMotionMoveToForce;

/** 점멸 — 스태프 전용 이동기(대시 대체) */
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

	// 경로 끝(BlinkDistance 지점)에 적이 겹치면 그 앞 스탠드오프 거리로 캡, 없으면 BlinkDistance (중간 적은 통과)
	float ComputeDashDistance(ACharacter* Character, const FVector& Start, const FVector& Dir, float CapsuleRadius, float CapsuleHalfHeight) const;

	// 대시 동안 캡슐의 Pawn 충돌을 Ignore로 했다가 복원
	void SetPawnCollisionIgnored(ACharacter* Character, bool bIgnore);
	
	void PlayArrivalMontage(ACharacter* Character) const;
	void StopArrivalMontage(ACharacter* Character) const;

	UFUNCTION()
	void OnDashFinished();

private:
	UPROPERTY(EditDefaultsOnly, Category = "Blink", meta = (ClampMin = "0.0"))
	float BlinkDistance = 600.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Blink", meta = (ClampMin = "1.0"))
	float DashSpeed = 6000.f;

	// 도착지 적 앞에 띄울 여유
	UPROPERTY(EditDefaultsOnly, Category = "Blink", meta = (ClampMin = "0.0"))
	float EnemyStandoffGap = 50.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Blink|Arrival")
	TSoftObjectPtr<UAnimMontage> ArrivalMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Blink|Arrival", meta = (ClampMin = "0.1"))
	float ArrivalMontagePlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Blink|Arrival", meta = (ClampMin = "0.0"))
	float ArrivalMontageBlendOut = 0.1f;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_ApplyRootMotionMoveToForce> DashTask;
	
	TEnumAsByte<ECollisionResponse> SavedPawnResponse = ECR_Block;
	bool bPawnIgnored = false;
};
