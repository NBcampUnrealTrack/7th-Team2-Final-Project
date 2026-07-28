#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RetrieveAttackHitLibrary.generated.h"

/**
 * 공격 Ability마다 IRetrieveAttackHitReceiver 검사 코드를 복제하지 않도록 하는 호출 유틸.
 * 서버 권한 구간에서만 호출할 것.
 */
UCLASS()
class RETRIEVE_API URetrieveAttackHitLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Target이 IRetrieveAttackHitReceiver를 구현하면 호출 후 그 반환값을 그대로 반환.
	// 구현하지 않으면 false(호출자가 기존 ASC 데미지 경로를 계속 진행해야 함).
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Combat")
	static bool TryNotifyAttackHitReceiver(
		AActor* Target,
		AActor* Attacker,
		const FHitResult& HitResult,
		FGameplayTag AttackTypeTag,
		FGameplayTag ElementTag);
};
