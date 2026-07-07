#pragma once

#include "CoreMinimal.h"
#include "Character/Cosmetics/RetrieveAlsLinkedAnimInstance.h"
#include "Character/Cosmetics/RetrieveBowMontageSet.h"

#include "RetrieveBowLinkedAnimInstance.generated.h"

/**
 * 활 장착 시 쓰는 '캐릭터' 상체 애님 레이어. base(URetrieveAlsLinkedAnimInstance)처럼
 * 캐릭터 메시에 레이어드된다 — 활 메시 ABP가 아니다(그건 URetrieveBowMeshAnimInstance).
 *
 * 활별(Cmp/Rcv/Lng)로 캐릭터 팔 포즈가 달라 서브클래스를 나누고, 무기 데이터의
 * UpperBodyAnimLayer로 지정되어 장착 시 링크된다. 활별 차징/발사 '캐릭터 몽타주'를 담아두고,
 * GA_BowShot이 이 레이어에서 꺼내 캐릭터 메인 AnimInstance에 재생한다
 * (base의 Draw/Equip 몽타주를 GA_Stance/EquipTransition이 꺼내 재생하는 것과 동일 패턴).
 * 활 사격 슬롯을 base에 두지 않는 이유 — 검/스태프 레이어 오염 방지.
 */
UCLASS()
class RETRIEVE_API URetrieveBowLinkedAnimInstance : public URetrieveAlsLinkedAnimInstance
{
	GENERATED_BODY()

public:
	// 이 활의 '캐릭터' 사격 phase 몽타주 세트.
	// (드로우 손 IK 노출은 메인 AnimInstance URetrieveAlsAnimInstance로 이관 — 링크 레이어는
	//  프리뷰에 부모 포즈가 없어 IK 저작이 안 되고, 런타임엔 ALS Control Rig가 하류에서 덮음)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|Bow|Shot")
	FRetrieveBowMontageSet ShotMontages;
};