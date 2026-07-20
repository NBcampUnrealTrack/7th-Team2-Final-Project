#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "AnimNotifyState_ActionLock.generated.h"

/**
 * 몽타주 구간 동안 지정 어빌리티 발동을 막는다(Begin=block / End=unblock).
 * 발사 후 방치되는 몽타주(패링 성공 리액션 등)도 어빌리티 수명과 무관하게 끝까지 안 끊기게 한다.
 */
UCLASS(DisplayName = "Action Lock (State)")
class RETRIEVE_API UAnimNotifyState_ActionLock : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAnimNotifyState_ActionLock();

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

	// 이 구간 동안 발동을 막을 어빌리티 태그. 기본=주요 전투 입력(공격/가드/패리/대시/블링크/버스트).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Combat")
	FGameplayTagContainer BlockedAbilityTags;
};
