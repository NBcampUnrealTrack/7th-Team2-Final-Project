#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "GA_RecallLumen.generated.h"

/**
 * 루멘을 호스트의 좌측 후방 위치로 소환합니다. Channel.Lumen.Command.Recall만 브로드캐스트하며
 * ULumenFollowComponent가 받아서 텔레포트를 실행합니다.
 */
UCLASS()
class RETRIEVE_API UGA_RecallLumen : public URetrieveGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_RecallLumen();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	                             const FGameplayAbilityActorInfo* ActorInfo,
	                             const FGameplayAbilityActivationInfo ActivationInfo,
	                             const FGameplayEventData* TriggerEventData) override;
};
