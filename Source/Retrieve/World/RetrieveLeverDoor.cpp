#include "World/RetrieveLeverDoor.h"

#include "World/RetrieveLeverActor.h"

ARetrieveLeverDoor::ARetrieveLeverDoor()
{
}

void ARetrieveLeverDoor::BeginPlay()
{
	Super::BeginPlay();

	for (ARetrieveLeverActor* Lever : Levers)
	{
		if (Lever)
		{
			Lever->OnLeverChanged.AddUniqueDynamic(this, &ARetrieveLeverDoor::HandleLeverChanged);
		}
	}

	// 시작 상태(bStartActivated 레버 등) 초기 평가.
	HandleLeverChanged(nullptr);
}

void ARetrieveLeverDoor::HandleLeverChanged(ARetrieveLeverActor* /*Lever*/)
{
	const bool bMet = IsConditionMet();

	// 조건 결과를 BP에 알림(서버·클라 모두 — 성공 연출용).
	OnConditionEvaluated(bMet);

	if (!HasAuthority())
	{
		return;
	}

	if (bMet)
	{
		OpenDoor();
	}
	else if (bRecloseWhenUnmet)
	{
		CloseDoor();
	}
}

bool ARetrieveLeverDoor::IsConditionMet() const
{
	if (Levers.Num() == 0)
	{
		return false;
	}

	switch (Condition)
	{
	case ERetrieveLeverDoorCondition::AllActivated:
		for (const ARetrieveLeverActor* Lever : Levers)
		{
			if (!Lever || !Lever->IsActivated())
			{
				return false;
			}
		}
		return true;

	case ERetrieveLeverDoorCondition::Pattern:
		for (int32 i = 0; i < Levers.Num(); ++i)
		{
			const bool bRequired = RequiredStates.IsValidIndex(i) ? RequiredStates[i] : false;
			if (!Levers[i] || Levers[i]->IsActivated() != bRequired)
			{
				return false;
			}
		}
		return true;
	}

	return false;
}
