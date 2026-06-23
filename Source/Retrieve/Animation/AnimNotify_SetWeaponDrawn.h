#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_SetWeaponDrawn.generated.h"

/**
 * 발검/납검 몽타주에서 '손이 무기를 잡는/놓는' 프레임에 배치하는 노티.
 * 소유 액터의 UWeaponComponent::SetWeaponDrawn을 호출해 무기 메시를 손/등 소켓으로 재부착한다.
 * (즉시 전환 — 공격發 발검·수영 입수 등 — 은 몽타주 없이 CombatStanceComponent가 직접 SetWeaponDrawn 호출)
 */
UCLASS(meta = (DisplayName = "Set Weapon Drawn"))
class RETRIEVE_API UAnimNotify_SetWeaponDrawn : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

protected:
	// true=손 소켓(발검 프레임), false=등 소켓(납검 프레임).
	UPROPERTY(EditAnywhere, Category = "Retrieve")
	bool bDrawn = true;
};
