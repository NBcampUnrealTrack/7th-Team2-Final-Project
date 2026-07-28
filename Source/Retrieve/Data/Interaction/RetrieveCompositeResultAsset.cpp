#include "Data/Interaction/RetrieveCompositeResultAsset.h"

void URetrieveCompositeResultAsset::ApplyResult_Implementation(
	UObject* WorldContextObject,
	AActor* Instigator,
	AActor* InteractedActor) const
{
	int32 AppliedCount = 0;
	for (URetrieveInteractionResultAsset* Child : ChildResults)
	{
		if (!Child)
		{
			continue;
		}
		// 재귀 가드 — 자기 자신을 자식으로 두면 무한 호출 방지
		if (Child == this)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Retrieve|Composite] %s가 자기 자신을 ChildResults에 포함 — 무한 호출 방지를 위해 스킵"),
				*GetName());
			continue;
		}

		Child->ApplyResult(WorldContextObject, Instigator, InteractedActor);
		++AppliedCount;
	}

	UE_LOG(LogTemp, Log,
		TEXT("[Retrieve|Composite] %s: %d개 자식 결과 적용"),
		*GetName(), AppliedCount);
}
