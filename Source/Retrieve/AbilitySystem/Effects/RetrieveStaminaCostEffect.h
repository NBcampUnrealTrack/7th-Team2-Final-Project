#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "RetrieveStaminaCostEffect.generated.h"

/**
 * 공용 스태미너 증감 GE(네이티브). Instant + 모디파이어 Stamina Additive = SetByCaller(Data.Cost.Stamina).
 *
 * 어빌리티 소모/회복, 가드 드레인, 자연 회복이 전부 이 GE 하나를 SetByCaller로 재사용
 */
UCLASS()
class RETRIEVE_API URetrieveStaminaCostEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	URetrieveStaminaCostEffect();
};
