#include "Character/Cosmetics/RetrieveBowMontageSet.h"

#include "Animation/AnimMontage.h"

UAnimMontage* FRetrieveBowMontageSet::Resolve(EBowShotPhase Phase, bool bCrouching) const
{
	const TSoftObjectPtr<UAnimMontage>* Stand = nullptr;
	const TSoftObjectPtr<UAnimMontage>* Crouch = nullptr;

	switch (Phase)
	{
	case EBowShotPhase::DrawnStart: Stand = &DrawnStartMontage; Crouch = &DrawnStartMontageCrouch; break;
	case EBowShotPhase::Drawn:      Stand = &DrawnMontage;      Crouch = &DrawnMontageCrouch;      break;
	case EBowShotPhase::DrawnShake: Stand = &DrawnShakeMontage; Crouch = &DrawnShakeMontageCrouch; break;
	case EBowShotPhase::FireReload: Stand = &FireReloadMontage; Crouch = &FireReloadMontageCrouch; break;
	case EBowShotPhase::FireIdle:   Stand = &FireIdleMontage;   Crouch = &FireIdleMontageCrouch;   break;
	case EBowShotPhase::Reload:     Stand = &ReloadMontage;     Crouch = &ReloadMontageCrouch;     break;
	default: return nullptr;
	}

	// 앉은 변형이 있으면 그것, 없으면 서있는 기본으로 폴백.
	const TSoftObjectPtr<UAnimMontage>& Pick = (bCrouching && !Crouch->IsNull()) ? *Crouch : *Stand;
	return Pick.LoadSynchronous();
}