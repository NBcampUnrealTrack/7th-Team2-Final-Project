#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_SetWeaponVisuals.generated.h"

/**
 * 장착/해제 몽타주에서 '무기가 나타나는/사라지는' 프레임에 배치하는 노티.
 * 같은 클래스를 한 몽타주에 두 번 꽂아 쓴다 (bSpawn 값만 다르게).
 *
 *   bSpawn=false (이른 프레임) → WeaponComponent::ClearWeaponVisuals  (기존/OLD 메시 제거)
 *   bSpawn=true  (잡는 프레임)  → WeaponComponent::SpawnWeaponVisuals  (현재 데이터로 NEW 스폰)
 *
 * 교체(스왑) 몽타주는 Clear → Spawn 순서를 지켜야 메시가 중복되지 않는다.
 * 시뮬레이트 프록시는 OnRep 즉시 스폰을 쓰므로 노티를 스킵한다(연출/즉시 중복 방지).
 * (소켓 스왑 = 발검/납검은 별개 노티 AnimNotify_SetWeaponDrawn)
 */
UCLASS(meta = (DisplayName = "Set Weapon Visuals"))
class RETRIEVE_API UAnimNotify_SetWeaponVisuals : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

protected:
	// true=메시 생성(잡는 프레임) / false=메시 파괴(이른 프레임).
	UPROPERTY(EditAnywhere, Category = "Retrieve")
	bool bSpawn = true;
};