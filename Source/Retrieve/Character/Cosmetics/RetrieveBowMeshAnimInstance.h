#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Character/Cosmetics/RetrieveBowMontageSet.h"

#include "RetrieveBowMeshAnimInstance.generated.h"

/**
 * 활 스켈레탈 메시의 AnimInstance (활별 BP 서브클래스가 각 활 메시에 지정된다).
 * 활 메시 자체(현/화살)의 phase 몽타주를 소유한다 —
 * 캐릭터 phase 몽타주(URetrieveBowLinkedAnimInstance)와 '짝'을 이루며,
 * GA_BowShot이 두 세트를 lockstep으로(같은 트리거) 재생해 프레임 싱크를 맞춘다.
 *
 * 손-현 공간 정합(Hand IK 등)은 ABP 구축 단계에서 확정한다.
 */
UCLASS()
class RETRIEVE_API URetrieveBowMeshAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	// 이 활 메시의 사격 phase 몽타주 세트(현/화살 움직임).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|Bow|Shot")
	FRetrieveBowMontageSet ShotMontages;
};