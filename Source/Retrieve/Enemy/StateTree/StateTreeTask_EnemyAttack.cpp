#include "Enemy/StateTree/StateTreeTask_EnemyAttack.h"

#include "StateTreeLinker.h"
#include "StateTreeExecutionContext.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayTags/RetrieveGameplayTags.h"

bool FStateTreeTask_EnemyAttack::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(PawnHandle);
	return true;
}

EStateTreeRunStatus FStateTreeTask_EnemyAttack::EnterState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime = 0.f;

	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);
	if (!Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}

	IAbilitySystemInterface* ASCInterface = Cast<IAbilitySystemInterface>(Pawn);
	if (!ASCInterface)
	{
		return EStateTreeRunStatus::Failed;
	}

	UAbilitySystemComponent* ASC = ASCInterface->GetAbilitySystemComponent();
	if (!ASC)
	{
		return EStateTreeRunStatus::Failed;
	}

	// GameplayEvent로 GA_EnemyBasicAttack 간접 활성화 (TryActivateAbilityByClass 금지 — Walkthrough §10.12)
	FGameplayEventData EventData;
	EventData.EventTag = RetrieveGameplayTags::GameplayEvent_Enemy_Attack;
	ASC->HandleGameplayEvent(RetrieveGameplayTags::GameplayEvent_Enemy_Attack, &EventData);

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_EnemyAttack::Tick(
	FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime += DeltaTime;

	// GA 활성화 직후 한 프레임 지연을 허용하기 위한 최소 대기
	if (InstanceData.ElapsedTime < 0.3f)
	{
		return EStateTreeRunStatus::Running;
	}

	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);
	if (Pawn)
	{
		IAbilitySystemInterface* ASCInterface = Cast<IAbilitySystemInterface>(Pawn);
		UAbilitySystemComponent* ASC = ASCInterface ? ASCInterface->GetAbilitySystemComponent() : nullptr;
		// GA_EnemyBasicAttack의 ActivationOwnedTags에서 State_Enemy_Attack을 관리
		// GA 종료 시 태그 자동 제거 → Succeeded 반환
		if (ASC && !ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Enemy_Attack))
		{
			return EStateTreeRunStatus::Succeeded;
		}
	}

	if (InstanceData.ElapsedTime >= InstanceData.MaxAttackDuration)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

void FStateTreeTask_EnemyAttack::ExitState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// TODO: UEnemyCombatComponent 구현 후 StopCurrentPattern() 호출로 교체
}
