#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Combat/RetrieveCombatTypes.h"
#include "RetrieveKnockbackLibrary.generated.h"

class ACharacter;

/**
 * 넉백 유틸 라이브러리. 상태 없음(stateless).
 * 다양한 상황의 넉백을 한 곳에 모은다:
 *   - 방향성(근접/투사체): ApplyKnockbackFromSource
 *   - 고정 방향(바람 등):  ApplyKnockbackInDirection
 *   - 방사형(범위/폭발):   ApplyRadialKnockback / ApplyRadialKnockbackToTargets
 *   - 자기발사(대시):      LaunchSelf
 *
 * 권한(Authority) 정책: 이 라이브러리는 권한 체크를 하지 않는다.
 *  - ACharacter::LaunchCharacter는 서버에서 호출되면 클라이언트로 자동 복제된다.
 *  - 피격자 넉백은 호출부가 서버 권한에서 호출해야 하고(기존 동작 유지),
 *    LocalPredicted 어빌리티(자기발사 등)의 클라 예측 여부는 호출부 판단에 맡긴다.
 */
UCLASS()
class RETRIEVE_API URetrieveKnockbackLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// [방향성] 공격 원점에서 피격자 반대 방향으로 밀친다. 근접/투사체용.
	// SourceLocation: 공격 원점(공격자 또는 투사체 위치). 방향 = (Target - Source).
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Knockback")
	static void ApplyKnockbackFromSource(ACharacter* Target, const FVector& SourceLocation, const FRetrieveKnockbackParams& Params);

	// [고정 방향] 지정한 월드 방향으로 피격자를 밀친다(바람 등).
	// WorldDirection은 내부에서 정규화된다. 수평만 원하면 호출 전에 Z를 0으로 둘 것.
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Knockback")
	static void ApplyKnockbackInDirection(ACharacter* Target, const FVector& WorldDirection, const FRetrieveKnockbackParams& Params);

	// [방사형] 중심+반경 안의 Pawn(ACharacter)을 수집해 각자 중심 바깥으로 밀친다.
	// 적용된 캐릭터 수를 반환. IgnoreActors는 수집에서 제외(보통 시전자).
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Knockback", meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "IgnoreActors"))
	static int32 ApplyRadialKnockback(const UObject* WorldContextObject, const FVector& Center, float Radius,
		const FRetrieveKnockbackParams& Params, const TArray<AActor*>& IgnoreActors);

	// [방사형/대상지정] 호출부가 이미 수집한 대상들에 방사형 넉백을 적용한다(중복 트레이스 회피).
	// Radius는 거리 감쇠(bScaleByDistance) 계산에만 쓰인다.
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Knockback")
	static void ApplyRadialKnockbackToTargets(const FVector& Center, float Radius, const TArray<ACharacter*>& Targets, const FRetrieveKnockbackParams& Params);

	// [자기발사] 시전자 자신을 지정 방향으로 발사한다. 대시/이동기용.
	// 기본 (bOverrideXY=true, bOverrideZ=false): 수평 속도는 덮어쓰고 수직(점프/낙하)은 보존.
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Knockback")
	static void LaunchSelf(ACharacter* Character, const FVector& WorldDirection, float Speed, bool bOverrideXY = true, bool bOverrideZ = false);

private:
	// 피격자 넉백: Root Motion Source(Override)로 적용 → StopMovementImmediately/락/AI 제어를 우회한다.
	static void DoKnockback(ACharacter* Target, const FVector& NormalizedDir, float Strength, float UpwardStrength, float Duration);

	// 자기발사(시전자 이동기): LaunchCharacter 임펄스. null/zero 가드.
	static void DoLaunch(ACharacter* Target, const FVector& NormalizedDir, float Strength, float UpwardStrength, bool bOverrideXY, bool bOverrideZ);
};
