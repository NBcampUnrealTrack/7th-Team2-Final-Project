#include "Animation/AnimNotifyState_RetrieveAttackWarp.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Combat/RetrieveTargetingLibrary.h"
#include "Components/Combat/CombatReactionComponent.h"
#include "GameFramework/Character.h"
#include "MotionWarpingComponent.h"
#include "RootMotionModifier.h"

URootMotionModifier* UAnimNotifyState_RetrieveAttackWarp::AddRootMotionModifier_Implementation(
	UMotionWarpingComponent* MotionWarpingComp, const UAnimSequenceBase* Animation, float StartTime, float EndTime) const
{
	// 엔진이 모디파이어를 만들기 직전에, 워프 타겟을 자동 해석해 등록한다.
	ResolveAndRegisterWarpTarget(MotionWarpingComp, Animation, StartTime, EndTime);

	// 엔진 기본 동작: 설정된 RootMotionModifier 생성/추가(위에서 등록한 타겟명을 참조).
	return Super::AddRootMotionModifier_Implementation(MotionWarpingComp, Animation, StartTime, EndTime);
}

void UAnimNotifyState_RetrieveAttackWarp::ResolveAndRegisterWarpTarget(
	UMotionWarpingComponent* MotionWarpingComp, const UAnimSequenceBase* Animation, float StartTime, float EndTime) const
{
	if (!IsValid(MotionWarpingComp))
	{
		return;
	}

	// 워프 타겟명은 디테일 패널에 설정된 모디파이어에서 그대로 가져온다(이름 이중 관리 불필요).
	const URootMotionModifier_Warp* WarpModifier = Cast<URootMotionModifier_Warp>(RootMotionModifier);
	if (!WarpModifier || WarpModifier->WarpTargetName.IsNone())
	{
		return;
	}
	const FName WarpName = WarpModifier->WarpTargetName;

	ACharacter* SourceChar = Cast<ACharacter>(MotionWarpingComp->GetOwner());
	if (!IsValid(SourceChar))
	{
		return;
	}

	if (const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(SourceChar))
	{
		if (const URetrieveAbilitySystemComponent* RetrieveASC = Cast<URetrieveAbilitySystemComponent>(ASC))
		{
			if (RetrieveASC->IsCounterWarpTargetLocked())
			{
				return;
			}
		}
	}

	const bool bPlayerControlled = SourceChar->IsPlayerControlled();

	// 1) 락온 타겟 우선
	AActor* Target = nullptr;
	if (const UCombatReactionComponent* CombatReaction = SourceChar->FindComponentByClass<UCombatReactionComponent>())
	{
		Target = CombatReaction->GetLockOnTarget();
	}

	// 2) 락온 없고 플레이어면 입력 방향 콘 검색
	if (!IsValid(Target) && bPlayerControlled)
	{
		Target = URetrieveTargetingLibrary::FindBestTarget(
			SourceChar, SearchRange, SearchHalfAngle,
			URetrieveTargetingLibrary::GetWarpAimDirection(SourceChar),
			MaxVerticalDelta, RangeWeightRate);
	}

	// 이 노티 윈도우의 루트모션 전진량 = 도약 상한(애님이 곧 사거리). 윈도우 = [StartTime, EndTime].
	float DashDistance = 0.f;
	if (const UAnimMontage* Montage = Cast<UAnimMontage>(Animation))
	{
		DashDistance = Montage->ExtractRootMotionFromTrackRange(StartTime, EndTime).GetTranslation().Size2D();
	}

	// 타겟 있음 → 타겟으로 회전 + standoff까지 접근(상한 클램프)
	if (IsValid(Target))
	{
		const FTransform WarpTransform =
			URetrieveTargetingLibrary::BuildWarpTransform(SourceChar, Target, StandoffOffset, DashDistance);
		MotionWarpingComp->AddOrUpdateWarpTargetFromTransform(WarpName, WarpTransform);
		return;
	}

	// 타겟 없음 + 플레이어 → '실제 이동 입력'이 있을 때만 그 방향으로 회전 + 전진.
	// 카메라 fallback(GetWarpAimDirection)을 여기 쓰면 무입력에도 정면으로 강제 회전하는 버그가 생긴다.
	// → 회전 워프는 raw 이동 입력만 사용. (콘 타겟 검색의 카메라 fallback은 위에서 그대로 유지)
	const FVector InputDir = SourceChar->GetLastMovementInputVector().GetSafeNormal2D();
	if (bRotateToInputWhenNoTarget && bPlayerControlled && !InputDir.IsNearlyZero())
	{
		const FVector WarpLocation = SourceChar->GetActorLocation() + InputDir * DashDistance;
		MotionWarpingComp->AddOrUpdateWarpTargetFromTransform(WarpName, FTransform(InputDir.Rotation(), WarpLocation));
		return;
	}

	// 입력 없음(또는 비플레이어) → 회전시키지 않음. 타겟을 비워 현재 facing 유지(생 루트모션 통과).
	MotionWarpingComp->RemoveWarpTarget(WarpName);
}
