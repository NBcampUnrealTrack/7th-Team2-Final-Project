#pragma once

#include "CoreMinimal.h"
#include "RetrieveCombatTypes.generated.h"

/**
 * 피격자 반응 강도
 *
 *  - Attack.Type.*   = "방어 처리"만 결정 (Guard / Parry / Shield 분기)
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
