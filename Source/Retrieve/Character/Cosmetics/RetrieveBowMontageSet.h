#pragma once

#include "CoreMinimal.h"
#include "RetrieveBowMontageSet.generated.h"

class UAnimMontage;

// 활 사격 차징/발사 phase. 캐릭터·활메시 몽타주를 phase 단위로 lockstep 재생할 때 쓴다.
UENUM()
enum class EBowShotPhase : uint8
{
	DrawnStart,   // 시위 당김(1회)
	Drawn,        // 당긴 채 대기(loop)
	DrawnShake,   // 풀차지 손떨림(loop)
	FireReload,   // 발사 + 재장전
	FireIdle,     // 발사 + 전탄 소진
	Reload,       // 빈 활 장전(획득 후 첫 드로우 전, 미노킹 → 노킹)
};

/**
 * 활 사격 phase 몽타주 세트 (서서 5 + 앉아서 5).
 * 캐릭터 레이어(URetrieveBowLinkedAnimInstance)와 활 메시 AnimInstance(URetrieveBowMeshAnimInstance)가
 * 각각 하나씩 보유한다 — GA_BowShot이 두 세트를 같은 phase로 lockstep 재생한다.
 */
USTRUCT(BlueprintType)
struct FRetrieveBowMontageSet
{
	GENERATED_BODY()

	// 서서 세트. Drawn/DrawnShake는 내부 루프 섹션 필요(정지할 때까지 순환).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Shot")
	TSoftObjectPtr<UAnimMontage> DrawnStartMontage;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Shot")
	TSoftObjectPtr<UAnimMontage> DrawnMontage;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Shot")
	TSoftObjectPtr<UAnimMontage> DrawnShakeMontage;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Shot")
	TSoftObjectPtr<UAnimMontage> FireReloadMontage;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Shot")
	TSoftObjectPtr<UAnimMontage> FireIdleMontage;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Shot")
	TSoftObjectPtr<UAnimMontage> ReloadMontage;       // 빈 활 장전(획득 후)

	// 앉아서 세트. 미지정 시 서서용으로 폴백.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Shot|Crouch")
	TSoftObjectPtr<UAnimMontage> DrawnStartMontageCrouch;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Shot|Crouch")
	TSoftObjectPtr<UAnimMontage> DrawnMontageCrouch;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Shot|Crouch")
	TSoftObjectPtr<UAnimMontage> DrawnShakeMontageCrouch;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Shot|Crouch")
	TSoftObjectPtr<UAnimMontage> FireReloadMontageCrouch;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Shot|Crouch")
	TSoftObjectPtr<UAnimMontage> FireIdleMontageCrouch;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Shot|Crouch")
	TSoftObjectPtr<UAnimMontage> ReloadMontageCrouch;

	// phase+자세에 맞는 몽타주 로드. Crouch 미지정 시 Stand 폴백. 둘 다 없으면 nullptr(호출부 스킵).
	UAnimMontage* Resolve(EBowShotPhase Phase, bool bCrouching) const;
};