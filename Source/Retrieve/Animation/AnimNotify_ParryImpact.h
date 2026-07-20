#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ParryImpact.generated.h"

/**
 * 패링 성공 몽타주의 "방패가 미는" 프레임에 배치 — 그 타이밍에 방금 패링한 적(PendingCounterTarget)에게
 * 타격+스태거를 적용한다. 데이터는 무기 DA(FWeaponParryData)에서 읽어 어빌리티 수명과 무관하다.
 */
UCLASS(DisplayName = "Parry Impact")
class RETRIEVE_API UAnimNotify_ParryImpact : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};
