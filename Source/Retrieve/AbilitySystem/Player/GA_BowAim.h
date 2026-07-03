#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "Components/Pawn/RetrieveCameraBoom.h"
#include "GA_BowAim.generated.h"

class UAbilityTask_WaitInputRelease;
/**
 * 
 */
UCLASS()
class RETRIEVE_API UGA_BowAim : public URetrieveGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_BowAim();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                                const FGameplayTagContainer* SourceTags = nullptr,
	                                const FGameplayTagContainer* TargetTags = nullptr,
	                                FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	// BowAim 동안 적용할 카메라 구도.
	// Weapon Data에 넣지 않고 GA에 둔다. 조준은 단순 발사가 아니라 레티클/차징/화살 UI까지 묶일
	// 운용 상태이므로, 장기적으로 BowAim 어빌리티가 카메라를 포함한 조준 경험을 소유하게 한다.
	UPROPERTY(EditDefaultsOnly, Category="Retrieve|Bow|Camera")
	FRetrieveCameraBoomProfile BowAimCameraProfile;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                             const FGameplayAbilityActivationInfo ActivationInfo,
	                             const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                        const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility,
	                        bool bWasCancelled) override;

private:
	UFUNCTION()
	void HandleInputReleased(float TimeHeld);

private:
	UPROPERTY(Transient)
	UAbilityTask_WaitInputRelease* WaitInputReleaseTask;

	// 조준 종료 시 같은 CameraBoom에 override 해제를 요청하기 위한 약참조.
	// 카메라 구도는 로컬 표현이므로 서버/원격 캐릭터에는 별도 상태를 남기지 않는다.
	UPROPERTY(Transient)
	TWeakObjectPtr<URetrieveCameraBoom> ActiveCameraBoom;
};
