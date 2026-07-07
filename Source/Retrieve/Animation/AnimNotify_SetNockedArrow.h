#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_SetNockedArrow.generated.h"

/**
 * 사격 몽타주에서 '노킹 화살이 보이는/사라지는' 프레임에 배치하는 노티.
 * 소유 액터의 UWeaponComponent::SetNockedArrowVisible을 호출한다.
 *
 * 탄약 분기는 GA_BowShot이 몽타주 선택(FireReload/FireIdle)으로 이미 처리하므로,
 * 이 노티는 표시/숨김만 담당한다(탄약 체크 없음). 캐릭터/활메시 어느 몽타주에 박아도
 * owner의 WeaponComponent를 찾으므로 동작한다.
 */
UCLASS(meta = (DisplayName = "Set Nocked Arrow"))
class RETRIEVE_API UAnimNotify_SetNockedArrow : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

protected:
	// true=화살 표시(노킹 프레임) / false=숨김(발사 프레임).
	UPROPERTY(EditAnywhere, Category = "Retrieve")
	bool bVisible = true;
};