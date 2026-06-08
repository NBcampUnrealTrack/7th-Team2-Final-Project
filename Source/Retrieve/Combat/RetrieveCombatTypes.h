#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "RetrieveCombatTypes.generated.h"

class AActor;

/**
 * 피격자 반응 강도
 *
 *  - Attack.Type.*   = "방어 처리"만 결정 (Guard / Parry 분기)
 *  - HitReact.Type.* = "피격 반응"만 결정 (Flinch / Stagger / Knockdown 모션)
 */
UENUM(BlueprintType)
enum class ERetrieveHitReactType : uint8
{
	None      UMETA(DisplayName = "None"),       // 반응 없음 (태그 미주입)
	Flinch    UMETA(DisplayName = "Flinch"),     // 가벼운 휘청
	Stagger   UMETA(DisplayName = "Stagger"),    // 강한 경직 + HitStop
	Knockdown UMETA(DisplayName = "Knockdown")   // 다운
};

// HitReact.Type.* 는 "피격 반응"만 결정 — Attack.Type(방어 판정)과 독립
inline FGameplayTag HitReactTypeToTag(ERetrieveHitReactType Type)
{
	switch (Type)
	{
	case ERetrieveHitReactType::Stagger:   return RetrieveGameplayTags::HitReact_Type_Stagger;
	case ERetrieveHitReactType::Knockdown: return RetrieveGameplayTags::HitReact_Type_Knockdown;
	case ERetrieveHitReactType::Flinch:    return RetrieveGameplayTags::HitReact_Type_Flinch;
	default:                               return FGameplayTag();
	}
}

/**
 * 패리 성공 시 UI / 카메라 / Cue 레이어에 broadcast 할 메시지 페이로드
 *
 *  - Instigator   : 책임 주체 (적 컨트롤러가 control하는 pawn)
 *  - EffectCauser : 직접 가해 actor (무기 / 투사체). 현재 melee에선 Instigator와 동일
 *  → projectile / 소환수 도입 시 둘이 갈라지므로 지금부터 분리
 */
USTRUCT(BlueprintType)
struct FRetrieveParryMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Combat|Parry")
	TObjectPtr<AActor> Victim = nullptr;        // 패리한 방어자

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Combat|Parry")
	TObjectPtr<AActor> Instigator = nullptr;    // 책임 주체(적 pawn)

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Combat|Parry")
	TObjectPtr<AActor> EffectCauser = nullptr;  // 직접 가해(무기/투사체)

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Combat|Parry")
	FGameplayTag AttackType;                    // Attack.Type.Normal / Heavy / Unblockable

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Combat|Parry")
	FGameplayEffectContextHandle Context;
};
