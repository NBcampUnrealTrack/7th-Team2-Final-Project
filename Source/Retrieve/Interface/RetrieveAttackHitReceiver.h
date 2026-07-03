#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "RetrieveAttackHitReceiver.generated.h"

UINTERFACE(BlueprintType)
class RETRIEVE_API URetrieveAttackHitReceiver : public UInterface
{
	GENERATED_BODY()
};

/**
 * ASC가 없는 월드 오브젝트(채집 광물 등)가 플레이어 공격을 받을 수 있게 하는 인터페이스.
 * 공격 Ability의 서버 권한 구간에서만 호출한다.
 */
class RETRIEVE_API IRetrieveAttackHitReceiver
{
	GENERATED_BODY()

public:
	/**
	 * @return true면 이 공격을 월드 오브젝트 피격으로 처리했음(기존 ASC 데미지 경로 생략).
	 *         false면 처리하지 않았으므로 호출자가 기존 ASC 처리 경로를 계속 진행해야 함.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Retrieve|Combat")
	bool ReceiveRetrieveAttackHit(
		AActor* Attacker,
		const FHitResult& HitResult,
		FGameplayTag AttackTypeTag,
		FGameplayTag ElementTag);
};
