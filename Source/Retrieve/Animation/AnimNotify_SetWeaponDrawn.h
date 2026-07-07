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

	// 비우면 전체 파트. 특정 손 소켓(DrawnSocket)을 지정하면 그 파트만 스왑한다
	// — 한 몽타주에서 검(prop_rSocket)·방패(Shield)를 다른 프레임에 옮길 때 사용.
	UPROPERTY(EditAnywhere, Category = "Retrieve")
	FName TargetDrawnSocket = NAME_None;

	// true면 소켓 스왑 대신 이 파트를 Hidden 처리한다(등 소켓 없는 방패 등).
	// 납검 애님과 동시에 숨기고 곧 ClearWeapon 노티가 파괴하는 용도.
	UPROPERTY(EditAnywhere, Category = "Retrieve")
	bool bSetHidden = false;
};
